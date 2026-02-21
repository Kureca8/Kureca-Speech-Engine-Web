/* it works a bit better idk why */
#include "tts_synth.h"
#include "lang_ru.h"
#include "lang_en.h"

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* forward declarations */
extern double      en_punctuation_pause(uint32_t cp);
extern char       *en_expand_input(const char *in);
extern int         en_grapheme_to_phonemes(const char *word, uint32_t *out, int max_out);
extern void        en_coarticulate_context(uint32_t prev, uint32_t cur, uint32_t next, PhonemeDef *out);
extern PhonemeDef *en_find_phoneme(uint32_t code);

/* limits */
#define MAX_UTF8_CP   8192
#define MAX_WORD      512
#define MAX_PHONES    512
#define FRAMES_PER_PH 8
#define MAX_FRAMES    (MAX_PHONES * FRAMES_PER_PH + 64)

#define MIN_FRAME_DUR  0.004

/* global state */
typedef enum { LANG_RU=0, LANG_EN=1, LANG_AUTO=2 } LangID;
static LangID  current_lang    = LANG_AUTO;
static double  tts_read_speed  = 1.0;
static double  prosody_base_f0 = 120.0;


/* POST-PROCESSING: normalise → LP → DC block → soft limiter */
typedef struct { float b0,b1,b2,a1,a2,x1,x2,y1,y2; } LPF2;
typedef struct { float x1,y1; } DCB;

static void lpf2_init(LPF2 *f, float fc, int sr)
{
	memset(f,0,sizeof(*f));
	float w0=2.0f*(float)M_PI*fc/(float)sr;
	float cw=cosf(w0),sw=sinf(w0),al=sw*0.7071f;
	float ai=1.0f/(1.0f+al);
	f->b0=(1.0f-cw)*0.5f*ai; f->b1=(1.0f-cw)*ai; f->b2=f->b0;
	f->a1=-2.0f*cw*ai; f->a2=(1.0f-al)*ai;
}
static inline float lpf2_proc(LPF2 *f, float x)
{
	float y=f->b0*x+f->b1*f->x1+f->b2*f->x2-f->a1*f->y1-f->a2*f->y2;
	f->x2=f->x1;f->x1=x;f->y2=f->y1;f->y1=y;return y;
}
static inline float dcb_proc(DCB *f, float x)
{ float y=x-f->x1+0.998f*f->y1;f->x1=x;f->y1=y;return y; }

static void postprocess(float *buf, int n, int sr)
{
	if(!buf||n<=0)return;
	/* peak normalise to 0.75 */
	float peak=0.0f;
	for(int i=0;i<n;i++){float a=fabsf(buf[i]);if(a>peak)peak=a;}
	if(peak>1e-6f){float s=0.75f/peak;for(int i=0;i<n;i++)buf[i]*=s;}
	/* 4th-order Butterworth LP at 7.5 kHz (2× 2nd-order) */
	LPF2 lp1,lp2; lpf2_init(&lp1,7500.f,sr); lpf2_init(&lp2,7500.f,sr);
	DCB dc={0,0};
	const float drive = 1.6f;
	const float inv_drive = 1.0f / drive;
	for(int i=0;i<n;i++){
		float s=lpf2_proc(&lp1,buf[i]);
		s=lpf2_proc(&lp2,s);
		s=dcb_proc(&dc,s);
		buf[i]=tanhf(s*drive)*inv_drive;
	}
}

/* FRAME HELPERS */
static inline void push_frame(FormantData *seq, int *idx,
							  const PhonemeDef *pd, uint32_t code,
							  double dur, uint32_t f0_hz)
{
	if(dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
	setup_formant(&seq[*idx], pd, code, dur);
	seq[*idx].dbg_code = f0_hz;
	(*idx)++;
}

/* interpolated frame (t 0→1) between phoneme a and b */
static void interp_frame(FormantData *seq, int *idx,
						 const PhonemeDef *a, const PhonemeDef *b,
						 double t, double dur, uint32_t f0_hz)
{
	PhonemeDef mid=*a;
	mid.f1=(int)(a->f1+(b->f1-a->f1)*t);
	mid.f2=(int)(a->f2+(b->f2-a->f2)*t);
	mid.f3=(int)(a->f3+(b->f3-a->f3)*t);
	mid.amp=(float)(a->amp+(b->amp-a->amp)*t);
	mid.duration=dur;
	push_frame(seq,idx,&mid,a->code,dur,f0_hz);
}

/* STOP TABLE  (Lisker & Abramson 1964 American English)
 * {closure_ms, aspiration_ms, f2_locus_hz, is_voiced} */
static const struct {
	float clos_ms, asp_ms;
	int   locus_f2;
	int   is_voiced;
} STOP_INFO[6] = {
	{58, 55,  700, 0},  /* /p/ — labial voiceless */
	{52,  0,  700, 1},  /* /b/ — labial voiced   */
	{62, 70, 1800, 0},  /* /t/ — alveolar voiceless */
	{56,  0, 1800, 1},  /* /d/ — alveolar voiced    */
	{68, 80, 2800, 0},  /* /k/ — velar voiceless */
	{62,  0, 2800, 1},  /* /g/ — velar voiced    */
};

static int stop_index(uint32_t code)
{
	/* EN_P=0xE020 ... EN_G=0xE025 */
	if(code>=0xE020u&&code<=0xE025u) return (int)(code-0xE020u);
	return -1;
}

/* EXPAND ONE PHONEME → 1–4 FormantData frames */
static void expand_phone(
	FormantData *seq, int *idx, int seq_cap,
	uint32_t code, const PhonemeDef *pd,
	const PhonemeDef *prev_pd, const PhonemeDef *next_pd,
	double dur_scale, double amp_scale, uint32_t f0_hz)
{
	if(*idx >= seq_cap-10) return;
	double spd = tts_read_speed;

	int si = stop_index(code);

	/* STOP */
	if(si >= 0){
		float clos = (float)(STOP_INFO[si].clos_ms * dur_scale / spd);
		float asp  = (float)(STOP_INFO[si].asp_ms  * dur_scale / spd);
		int   lf2  = STOP_INFO[si].locus_f2;
		int   isvd = STOP_INFO[si].is_voiced;

		if(clos < MIN_FRAME_DUR * 1000.f) clos = MIN_FRAME_DUR * 1000.f;

		if(isvd){
			/* voiced: voice bar during closure — louder, clearly periodic */
			PhonemeDef vb; memset(&vb,0,sizeof(vb));
			vb.code=code; vb.f1=160; vb.f2=lf2; vb.f3=2400;
			vb.duration=clos*0.001; vb.type=vtype_consonant;
			vb.amp=0.16f; vb.is_voiced=1;  /* was 0.11 */
			push_frame(seq,idx,&vb,code,vb.duration,f0_hz);
		} else {
			/* voiceless: silence closure */
			PhonemeDef cl; memset(&cl,0,sizeof(cl));
			cl.duration=clos*0.001; cl.type=vtype_silence; cl.amp=0.0f;
			push_frame(seq,idx,&cl,code,cl.duration,f0_hz);

			/* aspiration: shaped toward following vowel's formants.
			 * F1 is suppressed (divided by 2) — this is the key cue for voicelessness
			 * per Stevens & Klatt (1974). F2/F3 match following vowel. */
			if(asp>0.0f){
				if(asp < MIN_FRAME_DUR * 1000.f) asp = MIN_FRAME_DUR * 1000.f;
				PhonemeDef ap; memset(&ap,0,sizeof(ap));
				ap.code=0xE038u; /* HH */
				ap.f1=(next_pd&&next_pd->f1>100) ? next_pd->f1*2/5 : 350;  /* suppressed F1 */
				ap.f2=(next_pd&&next_pd->f2>200) ? next_pd->f2     : 1600;
				ap.f3=(next_pd&&next_pd->f3>200) ? next_pd->f3     : 2900;
				ap.duration=asp*0.001; ap.type=vtype_fricative;
				ap.amp=0.24f; ap.is_voiced=0;  /* was 0.19 */
				push_frame(seq,idx,&ap,ap.code,ap.duration,f0_hz);
			}
		}

		/* burst frame (very short — gives the transient click) */
		PhonemeDef bst=*pd;
		bst.duration=0.010/spd;  /* 10 ms, slightly longer than 8 ms */
		if(bst.duration < MIN_FRAME_DUR) bst.duration = MIN_FRAME_DUR;
		bst.amp=(float)(pd->amp*amp_scale*1.1);  /* slightly boosted */
		push_frame(seq,idx,&bst,code,bst.duration,f0_hz);
		return;
	}

	/* FRICATIVE: onset ramp (8ms) + main body + offset ramp (8ms) */
	if(pd->type==vtype_fricative){
		double total_dur = pd->duration * dur_scale / spd;
		double ramp = 0.008 / spd;
		if(ramp < MIN_FRAME_DUR) ramp = MIN_FRAME_DUR;
		if(total_dur < ramp*3) total_dur = ramp*3;
		double body_dur = total_dur - ramp*2;
		if(body_dur < MIN_FRAME_DUR) body_dur = MIN_FRAME_DUR;

		/* onset: quiet ramp in */
		PhonemeDef on=*pd; on.duration=ramp; on.amp=(float)(pd->amp*amp_scale*0.30);
		push_frame(seq,idx,&on,code,on.duration,f0_hz);
		/* main body: full amplitude */
		PhonemeDef body=*pd; body.duration=body_dur; body.amp=(float)(pd->amp*amp_scale);
		push_frame(seq,idx,&body,code,body.duration,f0_hz);
		/* offset: ramp out — prevents hard cut */
		PhonemeDef off=*pd; off.duration=ramp; off.amp=(float)(pd->amp*amp_scale*0.25);
		push_frame(seq,idx,&off,code,off.duration,f0_hz);
		return;
	}

	/* VOWEL / SONORANT */
	if(pd->type==vtype_vowel||pd->type==vtype_consonant){
		/* locus→vowel transition after stop: longer window (30ms) for clarity */
		if(prev_pd && stop_index(prev_pd->code)>=0){
			int lf2=STOP_INFO[stop_index(prev_pd->code)].locus_f2;
			PhonemeDef locus=*pd;
			locus.f2=lf2;
			locus.f1=pd->f1*2/5;  /* F1 starts very low at onset of voiced vowel */
			locus.amp=(float)(pd->amp*amp_scale*0.50);
			double tdur=0.030/spd;  /* was 0.022 */
			if(tdur < MIN_FRAME_DUR) tdur = MIN_FRAME_DUR;
			interp_frame(seq,idx,&locus,pd,0.50,tdur,f0_hz);
		}
		/* main frame */
		double dur=pd->duration*dur_scale/spd;
		if(dur<0.030)dur=0.030;
		PhonemeDef main_pd=*pd; main_pd.amp=(float)(pd->amp*amp_scale);
		push_frame(seq,idx,&main_pd,code,dur,f0_hz);
		return;
	}

	/* SILENCE / OTHER */
	double dur=pd->duration*dur_scale/spd;
	if(dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
	PhonemeDef main_pd=*pd; main_pd.amp=(float)(pd->amp*amp_scale);
	push_frame(seq,idx,&main_pd,code,dur,f0_hz);
}

/* STRESS HEURISTIC */
static int find_stress(const uint32_t *phones, int n)
{
	int best=-1; double bv=-1.0;
	for(int i=0;i<n;i++){
		PhonemeDef *p=en_find_phoneme(phones[i]);
		if(!p)continue;
		double v=p->duration*(p->type==vtype_vowel?1.6:0.4);
		if(v>bv){bv=v;best=i;}
	}
	return best;
}

/*PROSODY */
typedef struct { float f0; float dur_scale; float amp_scale; } Prosody;

static void compute_prosody(const uint32_t *phones, int n,
							int stress_idx, int is_question,
							float base_f0, Prosody *out)
{
	float decl_step = base_f0 * 0.008f;
	if(decl_step > 1.5f) decl_step = 1.5f;
	if(decl_step < 0.3f) decl_step = 0.3f;

	for(int i=0;i<n;i++){
		out[i].f0        = base_f0 - (float)i * decl_step;
		out[i].dur_scale = 1.0f;
		out[i].amp_scale = 1.0f;

		/* question rise: last 20% of phones */
		if(is_question && i>(int)(n*0.80f)){
			float qt=(float)(i-(int)(n*0.80f))/(float)(n*0.20f+1.f);
			out[i].f0 += qt * (base_f0 * 0.12f);
		}

		if(i==stress_idx){
			/* stressed vowel: bigger F0 peak, noticeably longer, louder */
			out[i].f0       +=12.0f;   /* was 9 */
			out[i].dur_scale=1.40f;   /* was 1.32 */
			out[i].amp_scale=1.15f;   /* was 1.10 */
		} else {
			PhonemeDef *p=en_find_phoneme(phones[i]);
			if(p&&p->type==vtype_vowel){
				/* unstressed vowels: shorter and quieter */
				out[i].dur_scale=0.75f;   /* was 0.82 */
				out[i].amp_scale=0.85f;   /* was 0.90 */
			}
		}

		/* phrase-final lengthening: last vowel before end gets longer */
		if(i == n-1 || i == n-2){
			PhonemeDef *p=en_find_phoneme(phones[i]);
			if(p&&p->type==vtype_vowel) out[i].dur_scale *= 1.20f;
		}

		float f0_floor = base_f0 * 0.40f;
		if(f0_floor < 65.f) f0_floor = 65.f;
		if(out[i].f0 < f0_floor) out[i].f0 = f0_floor;
	}
}

/* ENGLISH SEQUENCE BUILDER */
static TTSSeq *prepare_sequence_en(const uint32_t *norm, int ni)
{
	if(!norm||ni<=0)return NULL;
	FormantData *seq=(FormantData*)calloc((size_t)MAX_FRAMES,sizeof(FormantData));
	if(!seq)return NULL;
	int seq_len=0;

	int is_question=0;
	for(int i=0;i<ni;i++) if(norm[i]=='?'){is_question=1;break;}

	char     word_buf[MAX_WORD];
	int      wlen=0;
	uint32_t phones[MAX_PHONES];

	#define FLUSH_WORD() do { \
	if(wlen>0){ \
		word_buf[wlen]='\0'; \
		int nph=en_grapheme_to_phonemes(word_buf,phones,MAX_PHONES); \
		if(nph>0){ \
			int si=find_stress(phones,nph); \
			Prosody pr[MAX_PHONES]; \
			compute_prosody(phones,nph,si,is_question,(float)prosody_base_f0,pr); \
			for(int pi=0;pi<nph&&seq_len<MAX_FRAMES-12;pi++){ \
				PhonemeDef pd; \
				uint32_t pc=(pi>0)?phones[pi-1]:0; \
				uint32_t nc=(pi<nph-1)?phones[pi+1]:0; \
				en_coarticulate_context(pc,phones[pi],nc,&pd); \
				PhonemeDef ppd_s,npd_s,*ppd=NULL,*npd=NULL; \
				if(pc){en_coarticulate_context(0,pc,phones[pi],&ppd_s);ppd=&ppd_s;} \
					if(nc){en_coarticulate_context(phones[pi],nc,0,&npd_s);npd=&npd_s;} \
						expand_phone(seq,&seq_len,MAX_FRAMES, \
						phones[pi],&pd,ppd,npd, \
						pr[pi].dur_scale,pr[pi].amp_scale, \
						(uint32_t)roundf(pr[pi].f0)); \
			} \
		} \
		wlen=0; \
	} \
	} while(0)

	for(int i=0;i<ni&&seq_len<MAX_FRAMES-12;i++){
		uint32_t cp=norm[i];
		double psec=en_punctuation_pause(cp);
		if(psec>0.0||cp==0){
			FLUSH_WORD();
			if(psec>0.0){
				int ts=(int)ceil(psec*SAMPLE_RATE/tts_read_speed);
				if(ts<2)ts=2;
				seq[seq_len].sampleRate=SAMPLE_RATE;
				seq[seq_len].totalSamples=ts;
				seq[seq_len].type=vtype_silence;
				seq[seq_len].dbg_code=cp;
				seq_len++;
			}
			continue;
		}
		if(cp<0x80){
			char c=(char)cp;
			if(c>='A'&&c<='Z')c=(char)(c+32);
			if(c>='a'&&c<='z'){
				if(wlen<MAX_WORD-2) word_buf[wlen++]=c;
				else{FLUSH_WORD();word_buf[wlen++]=c;}
			} else FLUSH_WORD();
		} else FLUSH_WORD();
	}
	FLUSH_WORD();
	#undef FLUSH_WORD

	if(seq_len==0){free(seq);return NULL;}
	FormantData *s2=(FormantData*)realloc(seq,(size_t)seq_len*sizeof(FormantData));
	if(s2)seq=s2;
	TTSSeq *tts=(TTSSeq*)malloc(sizeof(TTSSeq));
	if(!tts){free(seq);return NULL;}
	tts->seq=seq; tts->seqLen=seq_len; tts->currentIndex=0;
	return tts;
}


/* RUSSIAN SEQUENCE (unchanged from original v2) */

typedef struct{uint32_t cp;int count;}RunEntry;
static int collapse_runs(const uint32_t *norm,int ni,RunEntry *runs,int rc)
{
	if(!norm||ni<=0||!runs||rc<=0)return 0;
	int ri=0;
	for(int i=0;i<ni;){
		uint32_t cp=norm[i];int cnt=1;
		while(i+cnt<ni&&norm[i+cnt]==cp)cnt++;
		if(ri<rc){runs[ri].cp=cp;runs[ri].count=cnt;ri++;}
		else runs[ri-1].count+=cnt;
		i+=cnt;
	}
	return ri;
}
static TTSSeq *prepare_sequence_ru(const uint32_t *norm,int ni)
{
	if(!norm||ni<=0)return NULL;
	int mr=ni+4;
	RunEntry *runs=(RunEntry*)malloc((size_t)mr*sizeof(RunEntry));
	if(!runs)return NULL;
	int nruns=collapse_runs(norm,ni,runs,mr);
	FormantData *seq=(FormantData*)calloc((size_t)nruns+8,sizeof(FormantData));
	if(!seq){free(runs);return NULL;}
	int sl=0;
	for(int i=0;i<nruns;i++){
		uint32_t cp=runs[i].cp;int cnt=runs[i].count;
		double ps=ru_punctuation_pause(cp);
		if(ps>0.0){
			int ts=(int)ceil(ps*cnt*SAMPLE_RATE/tts_read_speed);
			if(ts<2)ts=2;
			seq[sl].sampleRate=SAMPLE_RATE;seq[sl].totalSamples=ts;
			seq[sl].type=vtype_silence;seq[sl].dbg_code=cp;sl++;continue;
		}
		if(cp==0)continue;
		PhonemeDef *pd=NULL;
		for(int k=0;ru_phonemes[k].code!=0;k++) if(ru_phonemes[k].code==cp){pd=&ru_phonemes[k];break;}
		if(!pd){
			int ts=(int)ceil(0.04*SAMPLE_RATE/tts_read_speed);if(ts<2)ts=2;
			seq[sl].sampleRate=SAMPLE_RATE;seq[sl].totalSamples=ts;
			seq[sl].type=vtype_silence;seq[sl].dbg_code=cp;sl++;continue;
		}
		if(pd->type==vtype_silence&&cp==0x042C)continue;
		double dur = pd->duration*(double)cnt/tts_read_speed;
		if(dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
		setup_formant(&seq[sl],pd,cp,dur);sl++;
	}
	free(runs);
	if(sl==0){free(seq);return NULL;}
	TTSSeq *tts=(TTSSeq*)malloc(sizeof(TTSSeq));
	if(!tts){free(seq);return NULL;}
	tts->seq=seq;tts->seqLen=sl;tts->currentIndex=0;
	return tts;
}

/* LANG DETECT */
static LangID detect_lang(const char *txt)
{
	int ru=0,en=0;
	const unsigned char *s=(const unsigned char*)txt;
	while(*s){
		unsigned char c=*s;
		if(c<0x80){if((c>='A'&&c<='Z')||(c>='a'&&c<='z'))en++;s++;}
		else if((c&0xE0)==0xC0){if(s[0]==0xD0||s[0]==0xD1)ru++;if((s[1]&0xC0)==0x80)s+=2;else s++;}
		else if((c&0xF0)==0xE0){if(((s[1]&0xC0)==0x80)&&((s[2]&0xC0)==0x80))s+=3;else s++;}
		else if((c&0xF8)==0xF0){if(((s[1]&0xC0)==0x80)&&((s[2]&0xC0)==0x80)&&((s[3]&0xC0)==0x80))s+=4;else s++;}
		else s++;
	}
	if(!ru&&!en)return LANG_EN;
	return(en>ru)?LANG_EN:LANG_RU;
}

/*UTF-8 → CODEPOINTS */
static int utf8_to_cp(const char *s,uint32_t *o,int max)
{
	int i=0,n=0;if(!s||!o||max<=0)return 0;
	while(s[i]&&n<max){
		unsigned char c=(unsigned char)s[i];
		if(c<0x80){o[n++]=c;i++;}
		else if((c&0xE0)==0xC0){unsigned char c1=(unsigned char)s[i+1];
			if(c1&&(c1&0xC0)==0x80){o[n++]=(uint32_t)(((c&0x1F)<<6)|(c1&0x3F));i+=2;}else i++;}
			else if((c&0xF0)==0xE0){unsigned char c1=(unsigned char)s[i+1],c2=(unsigned char)s[i+2];
				if(c1&&c2&&((c1&0xC0)==0x80)&&((c2&0xC0)==0x80)){o[n++]=(uint32_t)(((c&0x0F)<<12)|((c1&0x3F)<<6)|(c2&0x3F));i+=3;}else i++;}
				else if((c&0xF8)==0xF0){unsigned char c1=(unsigned char)s[i+1],c2=(unsigned char)s[i+2],c3=(unsigned char)s[i+3];
					if(c1&&c2&&c3&&((c1&0xC0)==0x80)&&((c2&0xC0)==0x80)&&((c3&0xC0)==0x80)){
						o[n++]=((c&0x07)<<18)|((c1&0x3F)<<12)|((c2&0x3F)<<6)|(c3&0x3F);i+=4;}else i++;}
						else i++;
	}
	return n;
}


/* OUTPUT BUFFER */
static float *tts_output_buf=NULL;
static int    tts_output_len=0;


/* PUBLIC API */
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_sample_rate(void){return SAMPLE_RATE;}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_language(int lang)
{
	if(lang==1)current_lang=LANG_EN;
	else if(lang==2)current_lang=LANG_AUTO;
	else current_lang=LANG_RU;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_speed(double spd)
{ if(spd<0.1)spd=0.1;if(spd>8.0)spd=8.0;tts_read_speed=spd; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_pitch(double hz)
{ if(hz<50.0)hz=50.0;if(hz>300.0)hz=300.0;prosody_base_f0=hz; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_speak(const char *txt)
{
	if(!txt)return 0;
	static int seeded=0;
	if(!seeded){srand((unsigned int)time(NULL));seeded=1;}

	LangID eff=(current_lang==LANG_AUTO)?detect_lang(txt):current_lang;
	TTSSeq *s=NULL;

	if(eff==LANG_EN){
		char *exp=en_expand_input(txt);
		const char *use=exp?exp:txt;
		uint32_t *codes=(uint32_t*)malloc((size_t)MAX_UTF8_CP*sizeof(uint32_t));
		if(!codes){if(exp)free(exp);return 0;}
		int ni=utf8_to_cp(use,codes,MAX_UTF8_CP);
		if(exp)free(exp);
		if(ni>0)s=prepare_sequence_en(codes,ni);
		free(codes);
	} else {
		char *exp=ru_expand_input(txt);
		const char *use=exp?exp:txt;
		int half=MAX_UTF8_CP/2;
		uint32_t *codes=(uint32_t*)malloc((size_t)half*sizeof(uint32_t));
		uint32_t *norm =(uint32_t*)malloc((size_t)half*sizeof(uint32_t));
		if(!codes||!norm){free(codes);free(norm);if(exp)free(exp);return 0;}
		int ncp=utf8_to_cp(use,codes,half);
		if(exp)free(exp);
		int ni=0;
		for(int i=0;i<ncp&&ni<half;i++)norm[ni++]=ru_normalize_upper(codes[i]);
		free(codes);
		if(ni>0)s=prepare_sequence_ru(norm,ni);
		free(norm);
	}

	if(!s)return 0;

	long long total=0;
	for(int i=0;i<s->seqLen;i++)total+=(long long)s->seq[i].totalSamples;
	if(total<=0){free_tts(s);return 0;}
	if(total>SAMPLE_RATE*90)total=SAMPLE_RATE*90;

	if(tts_output_buf){free(tts_output_buf);tts_output_buf=NULL;tts_output_len=0;}
	tts_output_buf=(float*)malloc((size_t)total*sizeof(float));
	if(!tts_output_buf){free_tts(s);return 0;}

	reset_seq(s);
	int idx=0;
	while(s->currentIndex<s->seqLen&&idx<(int)total)
		tts_output_buf[idx++]=generate_sample(s);
	free_tts(s);

	tts_output_len=idx;
	postprocess(tts_output_buf,tts_output_len,SAMPLE_RATE);
	return tts_output_len;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
float *tts_get_buf(void){return tts_output_buf;}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_get_len(void){return tts_output_len;}
