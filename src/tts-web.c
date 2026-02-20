// formant based tts core
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 16000
#define TWO_PI (2.0 * M_PI)

/*
 * phoneme type
 * vtype_vowel      - voiced vowel modeled with 3 formants
 * vtype_consonant  - consonant, often has lower frequency formants and lp filtering
 * vtype_fricative  - noisy fricative band, modeled with bandpass + lp
 * vtype_stop       - plosive/stop, short burst then silence or voiced release
 * vtype_silence    - pause or silence
 */
typedef enum {
	vtype_vowel=0, vtype_consonant=1, vtype_fricative=2, vtype_stop=3, vtype_silence=4
} PhType;

/*
 * phoneme definition
 * code    : unicode codepoint for the character/phoneme (cyrillic uppercase used)
 * f1,f2,f3: formant center frequencies in hz (f3 may be zero for non-vowel)
 * duration: default dur in seconds for a single instance of phoneme
 * type    : phoneme class (vowel, consonant, fricative, stop, silence)
 * amp     : base amplitude multiplier for this phoneme
 * is_voiced: nonzero if phoneme contains voicing (periodic source), zero otherwise
 */
typedef struct {
	uint32_t code;
	double f1,f2,f3,duration;
	PhType type;
	double amp;
	int is_voiced;
} PhonemeDef;

/*
 * formant synthesis runtime data
 * contains biquad coeffs, state, envelope, glottal source state and helpers
 * this struct holds everything needed to render samples for one phoneme unit
 */
typedef struct {
	double sampleRate;
	int totalSamples, currentSample;
	PhType type;
	int is_voiced;
	double amplitude;
	double f[3];

	/* biquad coefficients for up to 3 bandpass filters */
	double b0[3],b1[3],b2[3],a1[3],a2[3];

	/* lowpass filter for fricatives / consonants */
	double lp_b0,lp_b1,lp_b2,lp_a1,lp_a2;
	double lp_x1,lp_x2,lp_y1,lp_y2;

	/* biquad state for each formant filter */
	double x1[3],x2[3],y1[3],y2[3];

	/* glottal/voicing state */
	double phase_f0;
	double pitch;
	double glottal_norm;
	int    glottal_max_h;

	/* noise / burst state */
	double noise_hp;
	int burstRemaining;

	/* simple attack / release envelope window */
	int attack_samples, release_samples;

	/* debug codepoint stored here for tracing */
	uint32_t dbg_code;
} FormantData;

/*
 * sequence container
 * seq     : array of formant data entries, one per run or pause
 * seqLen  : number of valid entries
 * currentIndex : index used during generation
 */
typedef struct {
	FormantData *seq;
	int seqLen, currentIndex;
} TTSSeq;

/* global read speed multiplier, can speed up or slow down generated durations */
static double READ_SPEED = 1.0;

/*
 * phoneme table
 * each line maps a unicode code (cyrillic) to acoustic parameters.
 * comments on each entry explain the sound and modeling notes.
 *
 * fields: { codepoint, f1, f2, f3, duration, type, amplitude, is_voiced }
 */
static PhonemeDef phonemes[] = {
	/* 0x0410 'а' - open central vowel, strong f1, f2, f3 typical for russian 'a' */
	{0x0410, 700, 1220,2600,0.15,vtype_vowel,    1.0, 1},
	/* 0x0415 'е' - mid front vowel, slightly higher f2 */
	{0x0415, 500, 1700,2500,0.13,vtype_vowel,    0.95,1},
	/* 0x0401 'ё' - similar to 'е', slightly adjusted for rounding */
	{0x0401, 500, 1700,2500,0.13,vtype_vowel,    0.95,1},
	/* 0x0418 'и' - close front vowel, low f1 high f2 */
	{0x0418, 300, 2200,2950,0.12,vtype_vowel,    0.9, 1},
	/* 0x041E 'о' - mid back vowel, lower f2 */
	{0x041E, 500,  900,2300,0.14,vtype_vowel,    0.95,1},
	/* 0x0423 'у' - close back vowel, low f1 and low f2 */
	{0x0423, 300,  870,2250,0.14,vtype_vowel,    0.9, 1},
	/* 0x042B 'ы' - high central vowel, f1/f2 intermediate */
	{0x042B, 400, 1400,2500,0.13,vtype_vowel,    0.88,1},
	/* 0x042D 'э' - open front vowel */
	{0x042D, 550, 1700,2400,0.13,vtype_vowel,    0.9, 1},
	/* 0x042E 'ю' - rounded front/back diphthong like 'u' with front colouring */
	{0x042E, 300,  900,2200,0.13,vtype_vowel,    0.9, 1},
	/* 0x042F 'я' - palatalized vowel, slightly higher f1/f2 */
	{0x042F, 600, 1500,2550,0.14,vtype_vowel,    0.92,1},

	/* consonants - voiced approximants and stops with formant-like resonances */
	/* 0x0411 'б' - voiced bilabial stop approximated with low formant content */
	{0x0411, 250,  700,   0,0.14,vtype_consonant, 0.55,1},
	/* 0x0412 'в' - voiced labiodental approximant/fricative blend */
	{0x0412, 800, 2000,   0,0.14,vtype_consonant, 0.50,1},
	/* 0x0413 'г' - voiced velar stop with low formant */
	{0x0413, 200,  600,   0,0.13,vtype_consonant, 0.50,1},
	/* 0x0414 'д' - voiced alveolar stop */
	{0x0414, 250,  500,   0,0.14,vtype_consonant, 0.52,1},

	/* fricatives - modeled mainly as filtered noise */
	/* 0x0416 'ж' - voiced postalveolar fricative, high frequency energy */
	{0x0416,2800, 3500,   0,0.14,vtype_fricative,  0.38,1},
	/* 0x0417 'з' - voiced alveolar fricative */
	{0x0417,2200, 3500,   0,0.15,vtype_fricative,  0.38,1},

	/* more consonants */
	/* 0x0419 'й' - palatal approximant, treated as voiced consonant with high f2 */
	{0x0419, 300, 2000,   0,0.11,vtype_consonant,  0.48,1},
	/* 0x041B 'л' - lateral approximant with resonance */
	{0x041B, 400,  900,   0,0.14,vtype_consonant,  0.55,1},
	/* 0x041C 'м' - bilabial nasal approximated as low formant consonant */
	{0x041C, 300,  900,   0,0.14,vtype_consonant,  0.58,1},
	/* 0x041D 'н' - alveolar nasal */
	{0x041D, 300, 1000,   0,0.14,vtype_consonant,  0.58,1},
	/* 0x0420 'р' - trilled or tapped r, modeled as voiced consonant */
	{0x0420, 500, 1500,   0,0.14,vtype_consonant,  0.52,1},

	/* stops (plosives) - include a short burst followed by silence or voiced release */
	/* 0x041F 'п' - voiceless bilabial stop, short burst */
	{0x041F, 200,  600,   0,0.10,vtype_stop,       0.45,0},
	/* 0x0422 'т' - voiceless alveolar stop */
	{0x0422, 300,  900,   0,0.10,vtype_stop,       0.45,0},
	/* 0x041A 'к' - voiceless velar stop */
	{0x041A, 800, 1500,   0,0.10,vtype_stop,       0.42,0},

	/* more fricatives and sibilants */
	/* 0x0424 'ф' - voiceless labiodental fricative, high frequency energy */
	{0x0424,2200, 4000,   0,0.14,vtype_fricative,   0.32,0},
	/* 0x0421 'с' - voiceless alveolar sibilant (hissy) */
	{0x0421,4000, 6000,   0,0.16,vtype_fricative,   0.30,0},
	/* 0x0425 'х' - voiceless velar fricative */
	{0x0425,2000, 3500,   0,0.15,vtype_fricative,   0.30,0},
	/* 0x0428 'ш' - voiceless postalveolar sibilant */
	{0x0428,2800, 4000,   0,0.16,vtype_fricative,   0.35,0},
	/* 0x0429 'щ' - long postalveolar sibilant, slightly different spectral shape */
	{0x0429,2800, 4200,   0,0.17,vtype_fricative,   0.35,0},

	/* affricates / stops with specific resonances */
	/* 0x0426 'ц' - voiceless alveolar affricate, modeled as stop+fricative */
	{0x0426,2000, 4000,   0,0.11,vtype_stop,        0.38,0},
	/* 0x0427 'ч' - voiceless postalveolar affricate */
	{0x0427,2200, 4000,   0,0.11,vtype_stop,        0.38,0},

	/* punctuation and soft silence symbols */
	/* 0x042A hard sign not used for sound here, treat as short silence */
	{0x042A,   0,   0,   0,0.03,vtype_silence,   0.0, 0},
	/* 0x042C soft sign: no sound, skip */
	{0x042C,   0,   0,   0,0.00,vtype_silence,   0.0, 0},
	/* terminator */
	{0,0,0,0,0,0,0,0}
};

/*
 * simple utf8 to codepoints converter
 * returns number of codepoints written to output array o (max m)
 */
static int utf8_to_codepoints(const char *s, uint32_t *o, int m) {
	int i=0,n=0;
	while(s[i]&&n<m){
		unsigned char c=(unsigned char)s[i];
		if      (c<0x80)           {o[n++]=c;i++;}
		else if ((c&0xE0)==0xC0)   {o[n++]=((c&0x1F)<<6)|((unsigned char)s[i+1]&0x3F);i+=2;}
		else if ((c&0xF0)==0xE0)   {o[n++]=((c&0x0F)<<12)|(((unsigned char)s[i+1]&0x3F)<<6)|((unsigned char)s[i+2]&0x3F);i+=3;}
		else i++;
	}
	return n;
}

/*
 * normalize to uppercase cyrillic
 * used to map lowercase input to table which stores uppercase codes
 */
static uint32_t normalize_upper(uint32_t cp){
	if(cp>=0x0430&&cp<=0x044F) return cp-0x20;
	if(cp==0x0451) return 0x0401;
	return cp;
}

/*
 * lookup phoneme definition by codepoint
 * returns pointer to phoneme def or null if not found
 */
static PhonemeDef *findPhonemeDef(uint32_t c){
	for(int i=0;phonemes[i].code!=0;i++)
		if(phonemes[i].code==c) return &phonemes[i];
		return NULL;
}

/*
 * punctuation pause mapping
 * return value is pause duration in seconds for a single punctuation character
 * these durations are short and intended to produce natural micro-pauses
 *
 * mapping:
 * space (32)        -> 0.08  : small gap between words
 * comma (44)        -> 0.15  : brief clause pause
 * period (46)       -> 0.28  : sentence end
 * exclamation/question (33/63) -> 0.32 : emphasized sentence end
 * semicolon/colon (59/58) -> 0.20 : medium pause
 * newline (10)     -> 0.35  : paragraph or line break pause
 * hyphen (45)      -> 0.07  : short connector pause
 */
static double punctuation_pause_seconds(uint32_t cp){
	if(cp==32)           return 0.08;
	if(cp==44)           return 0.15;
	if(cp==46)           return 0.28;
	if(cp==33||cp==63)   return 0.32;
	if(cp==59||cp==58)   return 0.20;
	if(cp==10)           return 0.35;
	if(cp==45)           return 0.07;
	return 0.0;
}

/*
 * expand_input_simple
 * very small text preprocessing:
 * - replace ascii digits 0..9 with russian words (zero to nine)
 * - preserve utf8 sequences
 * this keeps the tts module simple and predictable
 */
static char *expand_input_simple(const char *in){
	static const char *dw[10]={"ноль","один","два","три","четыре","пять","шесть","семь","восемь","девять"};
	size_t inl=strlen(in), outcap=(inl+1)*6+16;
	char *out=malloc(outcap); if(!out)return NULL; out[0]=0;
	size_t oi=0;
	for(size_t i=0;i<inl;){
		unsigned char c=(unsigned char)in[i];
		if(c<0x80){
			/* ascii path: convert digits to words, copy others as-is */
			if(c>='0'&&c<='9'){
				const char*w=dw[c-'0']; size_t wl=strlen(w);
				if(oi+wl+2>=outcap){ outcap*=2; out=realloc(out,outcap); }
				memcpy(out+oi,w,wl); oi+=wl; out[oi++]=' '; out[oi]=0; i++; continue;
			}
			if(oi+2>=outcap){ outcap*=2; out=realloc(out,outcap); }
			out[oi++]=in[i++]; out[oi]=0;
		} else {
			/* utf8 multi-byte copy */
			int bytes=1;
			if((c&0xE0)==0xC0) bytes=2; else if((c&0xF0)==0xE0) bytes=3; else if((c&0xF8)==0xF0) bytes=4;
			if(i+bytes>inl) bytes=(int)(inl-i);
			if(oi+bytes+2>=outcap){ outcap*=2; out=realloc(out,outcap); }
			memcpy(out+oi,in+i,bytes); oi+=bytes; out[oi]=0; i+=bytes;
		}
	}
	while(oi>0&&out[oi-1]==' '){ out[oi-1]=0; oi--; }
	return out;
}

/*
 * digital biquad initializers
 * init_bandpass: design a simple bandpass biquad using normalized formulas
 * init_lowpass:  design a second order butterworth-like lowpass
 *
 * these set filter coefficients used by apply_biquad and apply_lp
 */
static void init_bandpass(double fs,double f0,double Q,
						  double*b0,double*b1,double*b2,double*a1,double*a2){
	if(f0<=0.0||Q<=0.0){*b0=*b1=*b2=*a1=*a2=0.0;return;}
	double w0=TWO_PI*f0/fs, alpha=sin(w0)/(2.0*Q), cosw0=cos(w0), a0=1.0+alpha;
	*b0= alpha/a0; *b1=0.0; *b2=-alpha/a0;
	*a1=-2.0*cosw0/a0; *a2=(1.0-alpha)/a0;
						  }

						  static void init_lowpass(double fs, double fc,
												   double*b0,double*b1,double*b2,double*a1,double*a2){
							  double w0=TWO_PI*fc/fs, alpha=sin(w0)/(2.0*0.707), cosw0=cos(w0), a0=1.0+alpha;
							  *b0=(1.0-cosw0)/(2.0*a0); *b1=(1.0-cosw0)/a0; *b2=(1.0-cosw0)/(2.0*a0);
							  *a1=-2.0*cosw0/a0; *a2=(1.0-alpha)/a0;
												   }

												   /*
													* simple envelope function
													* returns attack/release shaped multiplier in range 0..1
													* attack_samples and release_samples are computed in setup_formant
													*/
												   static double envelope_amp(FormantData *d){
													   int n=d->currentSample, tail=d->totalSamples-n;
													   if(n    < d->attack_samples)  return (double)n   /(double)(d->attack_samples +1);
													   if(tail < d->release_samples) return (double)tail/(double)(d->release_samples+1);
													   return 1.0;
												   }

												   /*
													* glottal_source
													* crude voiced source using additive harmonics of a fundamental
													* glottal_max_h and glottal_norm are precomputed to normalize amplitude
													*/
												   static double glottal_source(FormantData *d){
													   double inc=TWO_PI*d->pitch/d->sampleRate;
													   d->phase_f0+=inc;
													   if(d->phase_f0>TWO_PI) d->phase_f0-=TWO_PI;
													   double src=0.0;
													   for(int h=1;h<=d->glottal_max_h;h++) src+=sin(d->phase_f0*h)/(double)h;
													   return (src/d->glottal_norm)*0.6;
												   }

												   /*
													* apply_biquad
													* standard direct form i implementation using stored state
													*/
												   static double apply_biquad(FormantData *d,int idx,double x){
													   double y=d->b0[idx]*x+d->b1[idx]*d->x1[idx]+d->b2[idx]*d->x2[idx]
													   - d->a1[idx]*d->y1[idx]-d->a2[idx]*d->y2[idx];
													   d->x2[idx]=d->x1[idx]; d->x1[idx]=x;
													   d->y2[idx]=d->y1[idx]; d->y1[idx]=y;
													   return y;
												   }

												   /*
													* apply_lp
													* apply lowpass biquad used for smoothing fricative noise and consonant outputs
													*/
												   static double apply_lp(FormantData *d, double x){
													   double y=d->lp_b0*x+d->lp_b1*d->lp_x1+d->lp_b2*d->lp_x2
													   - d->lp_a1*d->lp_y1- d->lp_a2*d->lp_y2;
													   d->lp_x2=d->lp_x1; d->lp_x1=x;
													   d->lp_y2=d->lp_y1; d->lp_y1=y;
													   return y;
												   }

												   /*
													* setup_formant
													* prepare formant data for a phoneme, compute filter coeffs and envelope lengths
													* duration_override allows elongating repeated characters into longer phoneme units
													*/
												   static void setup_formant(FormantData *d, PhonemeDef *pd, uint32_t dbg_code, double duration_override){
													   memset(d,0,sizeof(FormantData));
													   d->sampleRate=SAMPLE_RATE;
													   d->type=pd->type; d->is_voiced=pd->is_voiced; d->amplitude=pd->amp;
													   d->f[0]=pd->f1; d->f[1]=pd->f2; d->f[2]=pd->f3;
													   d->dbg_code=dbg_code;

													   double dur = (duration_override > 0.0) ? duration_override : pd->duration;
													   d->totalSamples=(int)(dur*SAMPLE_RATE/READ_SPEED);
													   if(d->totalSamples<2) d->totalSamples=2;

													   /* choose random pitch within reasonable range for natural variation */
													   d->pitch=90.0+(double)(rand()%41);
													   {
														   /* compute max harmonic number to avoid aliasing */
														   int max_h=(int)(d->sampleRate/(2.0*d->pitch));
														   if(max_h<1)max_h=1; if(max_h>40)max_h=40;
														   double norm=0.0;
														   for(int h=1;h<=max_h;h++) norm+=1.0/(double)h;
														   d->glottal_max_h=max_h;
														   d->glottal_norm=(norm>0.0)?norm:1.0;
													   }

													   d->phase_f0=0.0; d->noise_hp=0.0;
													   d->burstRemaining=(d->type==vtype_stop)?(int)(0.018*SAMPLE_RATE):0;

													   /* attack / release windows */
													   d->attack_samples = (int)(d->totalSamples * 0.14);
													   if(d->attack_samples < 3) d->attack_samples = 3;
													   d->release_samples = (int)(d->totalSamples * 0.22);
													   { int min_rel = (int)(0.018 * SAMPLE_RATE);
														   if(d->release_samples < min_rel) d->release_samples = min_rel; }
														   if(d->release_samples > d->totalSamples/2) d->release_samples = d->totalSamples/2;
														   if(d->release_samples < 2) d->release_samples = 2;

														   /* q scaling based on pitch to avoid overly sharp resonances at low pitch */
														   double q_scale=d->pitch/130.0;
														   if(q_scale>1.0)q_scale=1.0; if(q_scale<0.5)q_scale=0.5;

														   /* initialize filters depending on phoneme class */
														   if(d->type==vtype_vowel){
															   init_bandpass(SAMPLE_RATE,d->f[0],7.0*q_scale+1.0,&d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
															   init_bandpass(SAMPLE_RATE,d->f[1],9.0*q_scale+1.0,&d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
															   init_bandpass(SAMPLE_RATE,d->f[2],12.0*q_scale+1.0,&d->b0[2],&d->b1[2],&d->b2[2],&d->a1[2],&d->a2[2]);
														   } else if(d->type==vtype_fricative){
															   double fc1=(d->f[0]>0.0)?d->f[0]:3000.0;
															   double fc2=(d->f[1]>0.0)?d->f[1]:fc1*1.3;
															   init_bandpass(SAMPLE_RATE,fc1,2.5,&d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
															   init_bandpass(SAMPLE_RATE,fc2,2.0,&d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
															   double lp_fc = fc1 < 3500.0 ? fc1 * 0.80 : 2800.0;
															   init_lowpass(SAMPLE_RATE,lp_fc,&d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
														   } else if(d->type==vtype_consonant){
															   double fc1=(d->f[0]>0.0)?d->f[0]:400.0;
															   double fc2=(d->f[1]>0.0)?d->f[1]:1200.0;
															   init_bandpass(SAMPLE_RATE,fc1,5.0*q_scale+1.0,&d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
															   init_bandpass(SAMPLE_RATE,fc2,6.0*q_scale+1.0,&d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
															   init_lowpass(SAMPLE_RATE,3000.0,&d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
														   } else if(d->type==vtype_stop){
															   double fc1=(d->f[0]>0.0)?d->f[0]:600.0;
															   double fc2=(d->f[1]>0.0)?d->f[1]:1800.0;
															   init_bandpass(SAMPLE_RATE,fc1,3.5,&d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
															   init_bandpass(SAMPLE_RATE,fc2,3.0,&d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
															   init_lowpass(SAMPLE_RATE,2500.0,&d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
														   }
												   }

												   /*
													* run collapsing
													* compress consecutive identical codepoints into a single run entry
													* for voiced phonemes we allow run length > 1 to be elongated in duration
													* runs array is filled with { cp, count } pairs
													*/
												   typedef struct { uint32_t cp; int count; } RunEntry;

												   static int collapse_runs(const uint32_t *norm, int ni, RunEntry *runs){
													   int ri=0;
													   for(int i=0;i<ni;){
														   uint32_t cp=norm[i];
														   int cnt=1;
														   PhonemeDef *pd=NULL;
														   for(int k=0;phonemes[k].code!=0;k++) if(phonemes[k].code==cp){pd=&phonemes[k];break;}
														   /* if phoneme is sonic (not silence) then allow run collapsing */
														   if(pd && pd->type!=vtype_silence){
															   while(i+cnt<ni && norm[i+cnt]==cp) cnt++;
														   }
														   runs[ri].cp=cp; runs[ri].count=cnt; ri++;
														   i+=cnt;
													   }
													   return ri;
												   }

												   /*
													* prepare_sequence
													* main mapping from input utf8 text to a sequence of formant units
													* steps:
													* - convert utf8 to codepoints
													* - normalize to uppercase cyrillic
													* - collapse repeated letters into runs
													* - map punctuation to silence durations
													* - for each run, create a FormantData using setup_formant
													*
													* result is a tts sequence with seqLen entries ready for synthesis
													*/
												   static TTSSeq *prepare_sequence(const char *txt){
													   uint32_t codes[4096],norm[4096];
													   int ncp=utf8_to_codepoints(txt,codes,4096);
													   if(ncp<=0) return NULL;
													   int ni=0;
													   for(int i=0;i<ncp;i++) norm[ni++]=normalize_upper(codes[i]);

													   fprintf(stderr,"[tts] prepare_sequence: %d codepoints\n",ni);

													   RunEntry *runs=malloc((ni+4)*sizeof(RunEntry));
													   if(!runs) return NULL;
													   int nruns=collapse_runs(norm,ni,runs);

													   FormantData *seq=calloc(nruns+8,sizeof(FormantData));
													   if(!seq){free(runs);return NULL;}
													   int seqLen=0;

													   for(int i=0;i<nruns;i++){
														   uint32_t cp=runs[i].cp;
														   int cnt=runs[i].count;

														   /* punctuation handling: if cp maps to a pause length, create a silence entry
															*		   pause is multiplied by run count (for repeated punctuation) */
														   double psec=punctuation_pause_seconds(cp);
														   if(psec>0.0){
															   seq[seqLen].sampleRate=SAMPLE_RATE;
															   seq[seqLen].totalSamples=(int)(psec*cnt*SAMPLE_RATE/READ_SPEED);
															   if(seq[seqLen].totalSamples<2)seq[seqLen].totalSamples=2;
															   seq[seqLen].type=vtype_silence;
															   seq[seqLen].dbg_code=cp;
															   seqLen++; continue;
														   }
														   if(cp==0) continue;

														   /* find phoneme definition; if not found, insert a short silence to keep timing */
														   PhonemeDef *pd=findPhonemeDef(cp);
														   if(!pd){
															   seq[seqLen].sampleRate=SAMPLE_RATE;
															   seq[seqLen].totalSamples=(int)(0.04*SAMPLE_RATE/READ_SPEED);
															   seq[seqLen].type=vtype_silence;
															   seq[seqLen].dbg_code=cp;
															   seqLen++; continue;
														   }
														   /* skip soft sign which carries no sound in russian */
														   if(pd->type==vtype_silence&&cp==0x042C) continue;

														   /* duration is multiplied by run count so repeated letters become prolonged phoneme */
														   double total_dur = pd->duration * cnt;
														   setup_formant(&seq[seqLen],pd,cp,total_dur);
														   seqLen++;
													   }

													   free(runs);
													   fprintf(stderr,"[tts] seqLen=%d\n",seqLen);
													   if(seqLen==0){free(seq);return NULL;}
													   TTSSeq *tts=malloc(sizeof(TTSSeq));
													   tts->seq=seq; tts->seqLen=seqLen; tts->currentIndex=0;
													   return tts;
												   }

												   /*
													* softclip
													* gentle non-linear limiter to avoid hard clipping and keep perceived loudness
													*/
												   static double softclip(double x){
													   if(x >  1.0) return  1.0 - 1.0/(1.0+(x-1.0)*3.0);
													   if(x < -1.0) return -1.0 + 1.0/(1.0+(-x-1.0)*3.0);
													   return x;
												   }

												   /*
													* generate_sample
													* core sample generator that advances through sequence entries and synthesizes
													* each phoneme according to its type using glottal source and filtered noise
													*/
												   static float generate_sample(TTSSeq *tts){
													   if(!tts||tts->currentIndex>=tts->seqLen) return 0.0f;
													   FormantData *d=&tts->seq[tts->currentIndex];

													   while(d->currentSample>=d->totalSamples){
														   tts->currentIndex++;
														   if(tts->currentIndex>=tts->seqLen) return 0.0f;
														   d=&tts->seq[tts->currentIndex];
													   }

													   double env=envelope_amp(d);
													   double s=0.0;

													   if(d->type==vtype_silence){
														   s=0.0;
													   } else if(d->type==vtype_vowel){
														   /* voiced source filtered by three formant bandpasses and weighted */
														   double src=glottal_source(d);
														   double y0=apply_biquad(d,0,src);
														   double y1=apply_biquad(d,1,src);
														   double y2=apply_biquad(d,2,src);
														   s=(0.5*y0+1.0*y1+0.8*y2)*d->amplitude*env*0.9;
													   } else if(d->type==vtype_consonant){
														   /* consonant: voiced if flagged, add low-level noise for realism, then lp */
														   double src;
														   if(d->is_voiced){
															   src = glottal_source(d)*0.55
															   + ((double)rand()/(double)RAND_MAX*2.0-1.0)*0.02;
														   } else {
															   src = ((double)rand()/(double)RAND_MAX*2.0-1.0)*0.10;
														   }
														   double y0=apply_biquad(d,0,src);
														   double y1=apply_biquad(d,1,src);
														   s = apply_lp(d, y0*0.6+y1*0.4) * d->amplitude * env;
													   } else if(d->type==vtype_fricative){
														   /* fricative: primarily filtered noise, optionally mixed with voiced source */
														   double n=((double)rand()/(double)RAND_MAX*2.0-1.0);
														   double hp=n-0.88*d->noise_hp; d->noise_hp=n;
														   double y0=apply_biquad(d,0,hp);
														   double y1=apply_biquad(d,1,hp);
														   double mixed=y0*0.6+y1*0.4;
														   if(d->is_voiced) mixed=mixed*0.5+glottal_source(d)*0.35;
														   s = apply_lp(d, mixed) * d->amplitude * env;
													   } else if(d->type==vtype_stop){
														   /* stop/plosive: produce a short burst while burstRemaining > 0, then optionally voiced release */
														   if(d->burstRemaining>0){
															   double noise=((double)rand()/(double)RAND_MAX*2.0-1.0);
															   double n_hp=noise-0.85*d->noise_hp; d->noise_hp=noise;
															   double y0=apply_biquad(d,0,n_hp);
															   double y1=apply_biquad(d,1,n_hp);
															   double burst_env=(double)d->burstRemaining/(0.018*d->sampleRate);
															   s = apply_lp(d, (y0*0.5+y1*0.5)*burst_env*0.6);
															   d->burstRemaining--;
														   } else {
															   s=d->is_voiced?glottal_source(d)*0.25*d->amplitude*env:0.0;
														   }
													   }

													   if(!isfinite(s)) s=0.0;
													   d->currentSample++;
													   return (float)softclip(s*1.8);
												   }

												   /*
													* reset_seq
													* zero out runtime state before rendering so each play starts fresh
													*/
												   static void reset_seq(TTSSeq *tts){
													   tts->currentIndex=0;
													   for(int i=0;i<tts->seqLen;i++){
														   FormantData *d=&tts->seq[i];
														   d->currentSample=0; d->phase_f0=0.0; d->noise_hp=0.0;
														   d->lp_x1=d->lp_x2=d->lp_y1=d->lp_y2=0.0;
														   d->burstRemaining=(d->type==vtype_stop)?(int)(0.018*SAMPLE_RATE):0;
														   for(int j=0;j<3;j++) d->x1[j]=d->x2[j]=d->y1[j]=d->y2[j]=0.0;
													   }
												   }

												   /*
													* free_tts
													* free sequence memory
													*/
												   static void free_tts(TTSSeq *t){
													   if(!t)return;
													   if(t->seq)free(t->seq);
													   free(t);
												   }

												   /* buffer to hold final float32 output and its length */
												   static float *tts_output_buf=NULL;
												   static int    tts_output_len=0;

												   /* exported helpers for wasm/emscripten consumers */
												   #ifdef __EMSCRIPTEN__
												   EMSCRIPTEN_KEEPALIVE
												   #endif
												   int tts_sample_rate(void){ return SAMPLE_RATE; }

												   #ifdef __EMSCRIPTEN__
												   EMSCRIPTEN_KEEPALIVE
												   #endif
												   int tts_speak(const char *txt){
													   if(!txt) return 0;
													   static int seeded=0;
													   if(!seeded){srand((unsigned int)time(NULL));seeded=1;}

													   fprintf(stderr,"[tts] tts_speak: \"%s\"\n",txt);

													   /* expand simple tokens like digits into words to keep generation predictable */
													   char *expanded=expand_input_simple(txt);
													   const char *use=expanded?expanded:txt;

													   /* convert text to a prepared runtime sequence */
													   TTSSeq *s=prepare_sequence(use);
													   if(expanded)free(expanded);
													   if(!s) return 0;

													   /* compute total sample count and allocate output buffer */
													   int total=0;
													   for(int i=0;i<s->seqLen;i++) total+=s->seq[i].totalSamples;

													   if(tts_output_buf){free(tts_output_buf);tts_output_buf=NULL;}
													   tts_output_buf=malloc(total*sizeof(float));
													   if(!tts_output_buf){free_tts(s);return 0;}

													   /* render */
													   reset_seq(s);
													   int idx=0;
													   while(s->currentIndex<s->seqLen&&idx<total)
														   tts_output_buf[idx++]=generate_sample(s);

													   /* report peak amplitude for debugging or normalization */
													   float maxA=0.0f;
													   for(int i=0;i<idx;i++){float a=fabsf(tts_output_buf[i]);if(a>maxA)maxA=a;}
													   fprintf(stderr,"[tts] generated %d samples, maxA=%.4f\n",idx,maxA);

													   free_tts(s);
													   tts_output_len=idx;
													   return idx;
												   }

												   #ifdef __EMSCRIPTEN__
												   EMSCRIPTEN_KEEPALIVE
												   #endif
												   float *tts_get_buf(void){ return tts_output_buf; }
