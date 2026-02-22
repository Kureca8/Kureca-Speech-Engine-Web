// it works a bit better idk why
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

// forward declarations
extern double      en_punctuation_pause(uint32_t cp);
extern char       *en_expand_input(const char *in);
extern int         en_grapheme_to_phonemes(const char *word, uint32_t *out, int max_out);
extern void        en_coarticulate_context(uint32_t prev, uint32_t cur, uint32_t next, PhonemeDef *out);
extern PhonemeDef *en_find_phoneme(uint32_t code);
extern int         en_is_vowel(uint32_t c);
extern int         en_is_nasal(uint32_t c);
extern int         en_is_stop(uint32_t c);
extern int         en_is_fricative(uint32_t c);
extern int         en_is_sonorant(uint32_t c);
extern int         en_is_diphthong_onset(uint32_t c);

// limits
#define MAX_UTF8_CP   8192
#define MAX_WORD      512
#define MAX_PHONES    1024
#define MAX_FRAMES    (MAX_PHONES * 10 + 128)

#define MIN_FRAME_DUR  0.004

// global state
typedef enum { LANG_RU=0, LANG_EN=1, LANG_AUTO=2 } LangID;
static LangID  current_lang    = LANG_AUTO;
static double  tts_read_speed  = 1.0;
static double  prosody_base_f0 = 120.0;

static int whisper_mode = 0;

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_whisper(int enable)
{
	whisper_mode = (enable != 0) ? 1 : 0;
}

// biquad filter initialisation helpers
static void _init_bandpass_w(double fs, double f0, double Q,
							 double *b0, double *b1, double *b2,
							 double *a1, double *a2)
{
	if (f0 <= 0.0 || Q <= 0.0) { *b0=*b1=*b2=*a1=*a2=0.0; return; }
	double w0    = TWO_PI * f0 / fs;
	double alpha = sin(w0) / (2.0 * Q);
	double cosw0 = cos(w0);
	double a0    = 1.0 + alpha;
	*b0 =  alpha / a0;  *b1 = 0.0;  *b2 = -alpha / a0;
	*a1 = -2.0 * cosw0 / a0;
	*a2 = (1.0 - alpha) / a0;
}

static void _init_lowpass_w(double fs, double fc,
							double *b0, double *b1, double *b2,
							double *a1, double *a2)
{
	double w0    = TWO_PI * fc / fs;
	double alpha = sin(w0) / (2.0 * 0.707);
	double cosw0 = cos(w0);
	double a0    = 1.0 + alpha;
	*b0 = (1.0 - cosw0) / (2.0 * a0);
	*b1 = (1.0 - cosw0) / a0;
	*b2 = (1.0 - cosw0) / (2.0 * a0);
	*a1 = -2.0 * cosw0 / a0;
	*a2 = (1.0 - alpha) / a0;
}

// whisper transform
static void whisper_patch_frame(FormantData *fd)
{
	if (!fd) return;
	if (fd->type == vtype_silence) return;

	if (fd->type == vtype_vowel) {
		double f1w = fd->f[0] * 1.15;
		double f2w = fd->f[1] * 0.95;
		if (f1w > 1200.0) f1w = 1200.0;
		if (f2w > 3200.0) f2w = 3200.0;
		if (f1w < 100.0)  f1w = 100.0;
		if (f2w < 400.0)  f2w = 400.0;

		_init_bandpass_w(SAMPLE_RATE, f1w, 2.5,
						 &fd->b0[0], &fd->b1[0], &fd->b2[0], &fd->a1[0], &fd->a2[0]);
		_init_bandpass_w(SAMPLE_RATE, f2w, 2.0,
						 &fd->b0[1], &fd->b1[1], &fd->b2[1], &fd->a1[1], &fd->a2[1]);
		_init_lowpass_w(SAMPLE_RATE, f2w * 1.25,
						&fd->lp_b0, &fd->lp_b1, &fd->lp_b2, &fd->lp_a1, &fd->lp_a2);

		for (int j = 0; j < 3; j++) fd->x1[j]=fd->x2[j]=fd->y1[j]=fd->y2[j]=0.0;
		fd->lp_x1=fd->lp_x2=fd->lp_y1=fd->lp_y2=0.0;

		fd->type      = vtype_fricative;
		fd->is_voiced = 0;
		fd->amplitude *= 1.8;
		if (fd->amplitude > 1.0) fd->amplitude = 1.0;

	} else if (fd->type == vtype_consonant && fd->is_voiced) {
		fd->is_voiced  = 0;
		fd->amplitude *= 1.3;
		if (fd->amplitude > 1.0) fd->amplitude = 1.0;

	} else if (fd->type == vtype_stop && fd->is_voiced) {
		fd->is_voiced = 0;
	}
}

static void whisper_transform_seq(TTSSeq *seq)
{
	if (!seq || !seq->seq || seq->seqLen <= 0) return;
	for (int i = 0; i < seq->seqLen; i++)
		whisper_patch_frame(&seq->seq[i]);
}

// post-processing normalise -> lp -> dc block -> soft limiter
typedef struct { float b0,b1,b2,a1,a2,x1,x2,y1,y2; } LPF2;
typedef struct { float x1,y1; } DCB;

static void lpf2_init(LPF2 *f, float fc, int sr)
{
	memset(f, 0, sizeof(*f));
	float w0  = 2.0f * (float)M_PI * fc / (float)sr;
	float cw  = cosf(w0), sw = sinf(w0), al = sw * 0.7071f;
	float ai  = 1.0f / (1.0f + al);
	f->b0 = (1.0f - cw) * 0.5f * ai;
	f->b1 = (1.0f - cw) * ai;
	f->b2 = f->b0;
	f->a1 = -2.0f * cw * ai;
	f->a2 = (1.0f - al) * ai;
}

static inline float lpf2_proc(LPF2 *f, float x)
{
	float y = f->b0*x + f->b1*f->x1 + f->b2*f->x2 - f->a1*f->y1 - f->a2*f->y2;
	f->x2 = f->x1; f->x1 = x;
	f->y2 = f->y1; f->y1 = y;
	return y;
}

static inline float dcb_proc(DCB *f, float x)
{
	float y = x - f->x1 + 0.998f * f->y1;
	f->x1 = x; f->y1 = y;
	return y;
}

static void postprocess(float *buf, int n, int sr)
{
	if (!buf || n <= 0) return;

	// normalise to 0.75 peak
	float peak = 0.0f;
	for (int i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > peak) peak = a; }
	if (peak > 1e-6f) {
		float s = 0.75f / peak;
		for (int i = 0; i < n; i++) buf[i] *= s;
	}

	// gentle lp at 7500 hz remove aliasing artefacts
	LPF2 lp1, lp2;
	lpf2_init(&lp1, 7500.f, sr);
	lpf2_init(&lp2, 7500.f, sr);

	DCB dc = {0, 0};

	// soft limiter tanh with slight drive
	const float drive     = 1.4f;
	const float inv_drive = 1.0f / drive;

	for (int i = 0; i < n; i++) {
		float s = lpf2_proc(&lp1, buf[i]);
		s = lpf2_proc(&lp2, s);
		s = dcb_proc(&dc, s);
		buf[i] = tanhf(s * drive) * inv_drive;
	}
}

// frame helpers
static inline void push_frame(FormantData *seq, int *idx,
							  const PhonemeDef *pd, uint32_t code,
							  double dur, uint32_t f0_hz)
{
	if (dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
	setup_formant(&seq[*idx], (PhonemeDef *)pd, code, dur);
	seq[*idx].dbg_code = f0_hz;
	(*idx)++;
}

// interpolate two phoneme defs by t in [0,1] and push as one frame
static void interp_frame(FormantData *seq, int *idx,
						 const PhonemeDef *a, const PhonemeDef *b,
						 double t, double dur, uint32_t f0_hz)
{
	PhonemeDef mid = *a;
	mid.f1  = (int)(a->f1  + (b->f1  - a->f1)  * t);
	mid.f2  = (int)(a->f2  + (b->f2  - a->f2)  * t);
	mid.f3  = (int)(a->f3  + (b->f3  - a->f3)  * t);
	mid.amp = (float)(a->amp + (b->amp - a->amp) * t);
	mid.duration = dur;
	push_frame(seq, idx, &mid, a->code, dur, f0_hz);
}

// stop burst model
// closure voicing bar for voiced stops aspiration shaped by following vowel
// aspiration durations p t k given below burst transient parameters from literature
typedef struct {
	float  clos_ms;   // closure voicing bar duration
	float  asp_ms;    // aspiration vot
	int    locus_f2;  // f2 locus for formant transitions
	int    burst_f1;  // burst noise band lo
	int    burst_f2;  // burst noise band hi
	int    is_voiced;
} StopInfo;

// burst spectra notes bilabials alveolars velars used to build burst transient frame
static const StopInfo STOP_INFO[6] = {
	{55, 55,  800,  600, 1800, 0},   // p low burst
	{55,  0,  800,  600, 1800, 1},   // b
	{62, 70, 1800, 2800, 5000, 0},   // t high burst
	{58,  0, 1800, 2800, 5000, 1},   // d
	{70, 80, 2200, 1400, 3500, 0},   // k mid burst
	{65,  0, 2200, 1400, 3500, 1},   // g
};

static int stop_index(uint32_t code)
{
	if (code >= 0xE020u && code <= 0xE025u) return (int)(code - 0xE020u);
	return -1;
}

// affricate stop closure + fricative release
static int affricate_index(uint32_t code)
{
	if (code == EN_CH) return 0;
	if (code == EN_JH) return 1;
	return -1;
}

// expand_phone convert single phoneme into one or more formant frames
// handles stops affricates fricatives vowels sonorants diphthongs approximants and default fallback
static void expand_phone(
	FormantData *seq, int *idx, int seq_cap,
	uint32_t code, const PhonemeDef *pd,
	const PhonemeDef *prev_pd, const PhonemeDef *next_pd,
	double dur_scale, double amp_scale, uint32_t f0_hz)
{
	if (*idx >= seq_cap - 16) return;
	double spd = tts_read_speed;

	// stop consonants
	int si = stop_index(code);
	if (si >= 0) {
		float clos = (float)(STOP_INFO[si].clos_ms * dur_scale / spd);
		float asp  = (float)(STOP_INFO[si].asp_ms  * dur_scale / spd);
		int   lf2  = STOP_INFO[si].locus_f2;
		int   isvd = STOP_INFO[si].is_voiced;

		if (clos < MIN_FRAME_DUR * 1000.f) clos = MIN_FRAME_DUR * 1000.f;

		if (isvd) {
			// voiced stop closure low voicing bar f1 murmur ~200 hz
			PhonemeDef vb; memset(&vb, 0, sizeof(vb));
			vb.code = code; vb.f1 = 160; vb.f2 = lf2; vb.f3 = 2400;
			vb.duration = clos * 0.001; vb.type = vtype_consonant;
			vb.amp = 0.14f; vb.is_voiced = 1;
			push_frame(seq, idx, &vb, code, vb.duration, f0_hz);
		} else {
			// voiceless stop silence for closure
			PhonemeDef cl; memset(&cl, 0, sizeof(cl));
			cl.duration = clos * 0.001; cl.type = vtype_silence; cl.amp = 0.0f;
			push_frame(seq, idx, &cl, code, cl.duration, f0_hz);

			if (asp > 0.0f) {
				// aspiration noise shaped by following vowel formants
				if (asp < MIN_FRAME_DUR * 1000.f) asp = MIN_FRAME_DUR * 1000.f;
				PhonemeDef ap; memset(&ap, 0, sizeof(ap));
				ap.code = EN_HH;
				// take f1 f2 f3 from next vowel if available
				ap.f1 = (next_pd && next_pd->f1 > 100) ? (int)(next_pd->f1 * 0.7) : 380;
				ap.f2 = (next_pd && next_pd->f2 > 200) ? next_pd->f2              : 1600;
				ap.f3 = (next_pd && next_pd->f3 > 200) ? next_pd->f3              : 2800;
				ap.duration = asp * 0.001;
				ap.type = vtype_fricative;
				ap.amp  = 0.22f;
				ap.is_voiced = 0;
				push_frame(seq, idx, &ap, ap.code, ap.duration, f0_hz);
			}
		}

		// burst transient short frame with place specific spectral shape
		PhonemeDef bst; memset(&bst, 0, sizeof(bst));
		bst.code = code;
		bst.f1   = STOP_INFO[si].burst_f1;
		bst.f2   = STOP_INFO[si].burst_f2;
		bst.f3   = 0;
		bst.duration  = 0.009 / spd;
		if (bst.duration < MIN_FRAME_DUR) bst.duration = MIN_FRAME_DUR;
		bst.type      = vtype_fricative;   // noise source for burst
		bst.is_voiced = isvd;
		bst.amp       = (float)(pd->amp * amp_scale * 1.10);
		push_frame(seq, idx, &bst, code, bst.duration, f0_hz);
		return;
	}

	// affricates
	int ai = affricate_index(code);
	if (ai >= 0) {
		int is_voiced = (code == EN_JH);
		// closure
		double cl_dur = 0.045 * dur_scale / spd;
		if (cl_dur < MIN_FRAME_DUR) cl_dur = MIN_FRAME_DUR;
		if (is_voiced) {
			PhonemeDef vb; memset(&vb, 0, sizeof(vb));
			vb.code = code; vb.f1 = 180; vb.f2 = 1800; vb.f3 = 2600;
			vb.duration = cl_dur; vb.type = vtype_consonant;
			vb.amp = 0.15f; vb.is_voiced = 1;
			push_frame(seq, idx, &vb, code, vb.duration, f0_hz);
		} else {
			PhonemeDef cl; memset(&cl, 0, sizeof(cl));
			cl.duration = cl_dur; cl.type = vtype_silence; cl.amp = 0.0f;
			push_frame(seq, idx, &cl, code, cl.duration, f0_hz);
		}
		// fricative release sh-like
		double fr_dur = 0.080 * dur_scale / spd;
		if (fr_dur < MIN_FRAME_DUR) fr_dur = MIN_FRAME_DUR;
		PhonemeDef fr = *pd;
		fr.f1 = 1800; fr.f2 = 3500; fr.duration = fr_dur;
		fr.type = vtype_fricative;
		fr.amp  = (float)(pd->amp * amp_scale);
		push_frame(seq, idx, &fr, code, fr.duration, f0_hz);
		return;
	}

	// fricatives three frame envelope onset ramp steady state offset ramp
	if (pd->type == vtype_fricative) {
		double total = pd->duration * dur_scale / spd;
		double ramp  = 0.012 / spd;
		if (ramp < MIN_FRAME_DUR) ramp = MIN_FRAME_DUR;
		if (total < ramp * 3)  total = ramp * 3;
		double body  = total - ramp * 2;
		if (body < MIN_FRAME_DUR) body = MIN_FRAME_DUR;

		// clamp amp_scale to avoid making fricatives too loud
		double fric_amp = pd->amp * amp_scale;
		if (fric_amp > pd->amp * 1.05) fric_amp = pd->amp * 1.05;

		PhonemeDef on = *pd;
		on.duration = ramp; on.amp = (float)(fric_amp * 0.30);
		push_frame(seq, idx, &on, code, on.duration, f0_hz);

		PhonemeDef bd = *pd;
		bd.duration = body; bd.amp = (float)fric_amp;
		push_frame(seq, idx, &bd, code, bd.duration, f0_hz);

		PhonemeDef off = *pd;
		off.duration = ramp; off.amp = (float)(fric_amp * 0.20);
		push_frame(seq, idx, &off, code, off.duration, f0_hz);
		return;
	}

	// vowels and sonorant consonants
	if (pd->type == vtype_vowel || pd->type == vtype_consonant) {

		// locus transition from preceding stop insert short transition frame
		if (prev_pd && stop_index(prev_pd->code) >= 0) {
			int psi = stop_index(prev_pd->code);
			int lf2 = STOP_INFO[psi].locus_f2;

			PhonemeDef locus = *pd;
			locus.f2 = lf2;
			locus.f1 = (int)(pd->f1 * 0.50);
			locus.f3 = (pd->f3 > 0) ? (int)(pd->f3 * 0.92) : 2600;
			locus.amp = (float)(pd->amp * amp_scale * 0.55);
			double tdur = 0.025 / spd;
			if (tdur < MIN_FRAME_DUR) tdur = MIN_FRAME_DUR;
			// interpolate from locus to target in one frame
			interp_frame(seq, idx, &locus, pd, 0.6, tdur, f0_hz);
		}

		// main frame
		double dur = pd->duration * dur_scale / spd;
		if (dur < 0.030) dur = 0.030;
		PhonemeDef main_pd = *pd;
		main_pd.amp = (float)(pd->amp * amp_scale);
		main_pd.duration = dur;
		push_frame(seq, idx, &main_pd, code, dur, f0_hz);

		// diphthong onsets glide handling left to sequencer
		if (en_is_diphthong_onset(code)) {
			// no extra frames here glide phoneme follows in phones[]
		}

		return;
	}

	// default fallback
	double dur = pd->duration * dur_scale / spd;
	if (dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
	PhonemeDef fallback = *pd;
	fallback.amp = (float)(pd->amp * amp_scale);
	push_frame(seq, idx, &fallback, code, dur, f0_hz);
}

// stress assignment simplified rules based on espeak-ng cmu ideas
// returns index of stressed vowel phoneme in phones[]
static int find_stress(const uint32_t *phones, int n)
{
	// collect vowel positions
	int vpos[64]; int nv = 0;
	for (int i = 0; i < n && nv < 64; i++)
		if (en_is_vowel(phones[i]) && phones[i] != EN_AX &&
			phones[i] != EN_EY2 && phones[i] != EN_AY2 &&
			phones[i] != EN_AW2 && phones[i] != EN_OW2 && phones[i] != EN_OI2)
			vpos[nv++] = i;

		if (nv == 0) {
			// find anything vowel-like
			for (int i = 0; i < n; i++) { PhonemeDef *p = en_find_phoneme(phones[i]); if (p && p->type==vtype_vowel) return i; }
			return 0;
		}
		if (nv == 1) return vpos[0];
		if (nv == 2) {
			// default trochee first vowel stressed but check last vowel for reduced quality
			uint32_t lv = phones[vpos[nv-1]];
			if (lv == EN_ER || lv == EN_IH || lv == EN_AX)
				return vpos[0];
			// otherwise last vowel stressed iambic
			return vpos[nv-1];
		}
		// 3+ syllables antepenultimate default
		int stress_v = nv - 2;
		// if last vowel is full not reduced stress it
		uint32_t lv = phones[vpos[nv-1]];
		if (lv == EN_EY || lv == EN_AY || lv == EN_OW || lv == EN_AO || lv == EN_IY)
			stress_v = nv - 1;
	if (stress_v < 0) stress_v = 0;
	return vpos[stress_v];
}

// prosody model hat pattern onset nucleus declination questions unstressed rules
typedef struct { float f0; float dur_scale; float amp_scale; } Prosody;

static void compute_prosody(const uint32_t *phones, int n,
							int stress_idx, int is_question,
							float base_f0, Prosody *out)
{
	// find f0 peak position stress_idx vowel
	float f0_peak = base_f0 * 1.10f;
	float f0_floor = base_f0 * 0.72f;
	if (f0_floor < 70.f) f0_floor = 70.f;

	for (int i = 0; i < n; i++) {
		// default linear declination
		float t = (n > 1) ? (float)i / (float)(n - 1) : 0.f;
		float f0 = f0_peak - (f0_peak - f0_floor) * t;

		out[i].dur_scale = 1.0f;
		out[i].amp_scale = 1.0f;

		// stressed syllable pitch peak longer louder
		if (i == stress_idx) {
			f0 = f0_peak;
			out[i].dur_scale = 1.35f;
			out[i].amp_scale = 1.18f;
		} else {
			// unstressed vowels compress pitch shorten quieter
			PhonemeDef *p = en_find_phoneme(phones[i]);
			if (p && p->type == vtype_vowel && phones[i] != EN_AX) {
				out[i].dur_scale = 0.78f;
				out[i].amp_scale = 0.88f;
				f0 *= 0.96f;
			}
		}

		// schwa always short quiet flat
		if (phones[i] == EN_AX) {
			out[i].dur_scale = 0.60f;
			out[i].amp_scale = 0.72f;
		}

		// word final lengthening last vowel before end
		if (i == n-1 || i == n-2) {
			PhonemeDef *p = en_find_phoneme(phones[i]);
			if (p && p->type == vtype_vowel)
				out[i].dur_scale *= 1.15f;
		}

		// question f0 rises on last 15 percent of utterance
		if (is_question && i > (int)(n * 0.85f)) {
			float qt = (float)(i - (int)(n * 0.85f)) / (float)(n * 0.15f + 1.f);
			f0 += qt * base_f0 * 0.18f;
		}

		// clamp
		if (f0 < f0_floor) f0 = f0_floor;
		if (f0 > base_f0 * 1.40f) f0 = base_f0 * 1.40f;
		out[i].f0 = f0;
	}
}

// english sequence builder
static TTSSeq *prepare_sequence_en(const uint32_t *norm, int ni)
{
	if (!norm || ni <= 0) return NULL;

	FormantData *seq = (FormantData *)calloc((size_t)MAX_FRAMES, sizeof(FormantData));
	if (!seq) return NULL;
	int seq_len = 0;

	int is_question = 0;
	for (int i = 0; i < ni; i++) if (norm[i] == '?') { is_question = 1; break; }

	char     word_buf[MAX_WORD];
	int      wlen = 0;
	uint32_t phones[MAX_PHONES];

	#define FLUSH_WORD() do { \
	if (wlen > 0) { \
		word_buf[wlen] = '\0'; \
		int nph = en_grapheme_to_phonemes(word_buf, phones, MAX_PHONES); \
		if (nph > 0) { \
			int si = find_stress(phones, nph); \
			Prosody pr[MAX_PHONES]; \
			compute_prosody(phones, nph, si, is_question, (float)prosody_base_f0, pr); \
			for (int pi = 0; pi < nph && seq_len < MAX_FRAMES - 18; pi++) { \
				PhonemeDef pd; \
				uint32_t pc = (pi > 0)       ? phones[pi-1] : 0; \
				uint32_t nc = (pi < nph - 1)  ? phones[pi+1] : 0; \
				en_coarticulate_context(pc, phones[pi], nc, &pd); \
				PhonemeDef ppd_s, npd_s, *ppd = NULL, *npd = NULL; \
				if (pc) { en_coarticulate_context(0, pc, phones[pi], &ppd_s); ppd = &ppd_s; } \
					if (nc) { en_coarticulate_context(phones[pi], nc, 0, &npd_s); npd = &npd_s; } \
						expand_phone(seq, &seq_len, MAX_FRAMES, \
						phones[pi], &pd, ppd, npd, \
						pr[pi].dur_scale, pr[pi].amp_scale, \
						(uint32_t)roundf(pr[pi].f0)); \
			} \
		} \
		wlen = 0; \
	} \
	} while (0)

	for (int i = 0; i < ni && seq_len < MAX_FRAMES - 18; i++) {
		uint32_t cp = norm[i];
		double psec = en_punctuation_pause(cp);
		if (psec > 0.0 || cp == 0) {
			FLUSH_WORD();
			if (psec > 0.0) {
				int ts = (int)ceil(psec * SAMPLE_RATE / tts_read_speed);
				if (ts < 2) ts = 2;
				seq[seq_len].sampleRate   = SAMPLE_RATE;
				seq[seq_len].totalSamples = ts;
				seq[seq_len].type         = vtype_silence;
				seq[seq_len].dbg_code     = cp;
				seq_len++;
			}
			continue;
		}
		if (cp < 0x80) {
			char c = (char)cp;
			if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
			if (c >= 'a' && c <= 'z') {
				if (wlen < MAX_WORD - 2) word_buf[wlen++] = c;
				else { FLUSH_WORD(); word_buf[wlen++] = c; }
			} else FLUSH_WORD();
		} else FLUSH_WORD();
	}
	FLUSH_WORD();
	#undef FLUSH_WORD

	if (seq_len == 0) { free(seq); return NULL; }

	FormantData *s2 = (FormantData *)realloc(seq, (size_t)seq_len * sizeof(FormantData));
	if (s2) seq = s2;

	TTSSeq *tts = (TTSSeq *)malloc(sizeof(TTSSeq));
	if (!tts) { free(seq); return NULL; }
	tts->seq = seq; tts->seqLen = seq_len; tts->currentIndex = 0;
	return tts;
}

// russian sequence builder unchanged logic preserved
typedef struct { uint32_t cp; int count; } RunEntry;

static int collapse_runs(const uint32_t *norm, int ni, RunEntry *runs, int rc)
{
	if (!norm || ni <= 0 || !runs || rc <= 0) return 0;
	int ri = 0;
	for (int i = 0; i < ni; ) {
		uint32_t cp = norm[i]; int cnt = 1;
		while (i + cnt < ni && norm[i + cnt] == cp) cnt++;
		if (ri < rc) { runs[ri].cp = cp; runs[ri].count = cnt; ri++; }
		else runs[ri-1].count += cnt;
		i += cnt;
	}
	return ri;
}

static TTSSeq *prepare_sequence_ru(const uint32_t *norm, int ni)
{
	if (!norm || ni <= 0) return NULL;
	int mr = ni + 4;
	RunEntry *runs = (RunEntry *)malloc((size_t)mr * sizeof(RunEntry));
	if (!runs) return NULL;
	int nruns = collapse_runs(norm, ni, runs, mr);

	FormantData *seq = (FormantData *)calloc((size_t)nruns + 8, sizeof(FormantData));
	if (!seq) { free(runs); return NULL; }
	int sl = 0;

	for (int i = 0; i < nruns; i++) {
		uint32_t cp = runs[i].cp; int cnt = runs[i].count;
		double ps = ru_punctuation_pause(cp);
		if (ps > 0.0) {
			int ts = (int)ceil(ps * cnt * SAMPLE_RATE / tts_read_speed);
			if (ts < 2) ts = 2;
			seq[sl].sampleRate = SAMPLE_RATE; seq[sl].totalSamples = ts;
			seq[sl].type = vtype_silence; seq[sl].dbg_code = cp; sl++;
			continue;
		}
		if (cp == 0) continue;
		PhonemeDef *pd = NULL;
		for (int k = 0; ru_phonemes[k].code != 0; k++)
			if (ru_phonemes[k].code == cp) { pd = &ru_phonemes[k]; break; }
			if (!pd) {
				int ts = (int)ceil(0.04 * SAMPLE_RATE / tts_read_speed); if (ts < 2) ts = 2;
				seq[sl].sampleRate = SAMPLE_RATE; seq[sl].totalSamples = ts;
				seq[sl].type = vtype_silence; seq[sl].dbg_code = cp; sl++;
				continue;
			}
			if (pd->type == vtype_silence && cp == 0x042C) continue;
			double dur = pd->duration * (double)cnt / tts_read_speed;
		if (dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
		setup_formant(&seq[sl], pd, cp, dur); sl++;
	}
	free(runs);
	if (sl == 0) { free(seq); return NULL; }

	TTSSeq *tts = (TTSSeq *)malloc(sizeof(TTSSeq));
	if (!tts) { free(seq); return NULL; }
	tts->seq = seq; tts->seqLen = sl; tts->currentIndex = 0;
	return tts;
}

// language detection simple ru/en counters using utf8 patterns
static LangID detect_lang(const char *txt)
{
	int ru = 0, en = 0;
	const unsigned char *s = (const unsigned char *)txt;
	while (*s) {
		unsigned char c = *s;
		if (c < 0x80) {
			if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) en++;
			s++;
		} else if ((c & 0xE0) == 0xC0) {
			if (s[0] == 0xD0 || s[0] == 0xD1) ru++;
			if ((s[1] & 0xC0) == 0x80) s += 2; else s++;
		} else if ((c & 0xF0) == 0xE0) {
			if (((s[1] & 0xC0) == 0x80) && ((s[2] & 0xC0) == 0x80)) s += 3; else s++;
		} else if ((c & 0xF8) == 0xF0) {
			if (((s[1]&0xC0)==0x80)&&((s[2]&0xC0)==0x80)&&((s[3]&0xC0)==0x80)) s += 4; else s++;
		} else s++;
	}
	if (!ru && !en) return LANG_EN;
	return (en > ru) ? LANG_EN : LANG_RU;
}

// utf-8 to codepoint array
static int utf8_to_cp(const char *s, uint32_t *o, int max)
{
	int i = 0, n = 0;
	if (!s || !o || max <= 0) return 0;
	while (s[i] && n < max) {
		unsigned char c = (unsigned char)s[i];
		if (c < 0x80) { o[n++] = c; i++; }
		else if ((c & 0xE0) == 0xC0) {
			unsigned char c1 = (unsigned char)s[i+1];
			if (c1 && (c1 & 0xC0) == 0x80) { o[n++] = (uint32_t)(((c&0x1F)<<6)|(c1&0x3F)); i+=2; } else i++;
		} else if ((c & 0xF0) == 0xE0) {
			unsigned char c1=(unsigned char)s[i+1], c2=(unsigned char)s[i+2];
			if (c1&&c2&&((c1&0xC0)==0x80)&&((c2&0xC0)==0x80)) { o[n++]=(uint32_t)(((c&0x0F)<<12)|((c1&0x3F)<<6)|(c2&0x3F)); i+=3; } else i++;
		} else if ((c & 0xF8) == 0xF0) {
			unsigned char c1=(unsigned char)s[i+1],c2=(unsigned char)s[i+2],c3=(unsigned char)s[i+3];
			if (c1&&c2&&c3&&((c1&0xC0)==0x80)&&((c2&0xC0)==0x80)&&((c3&0xC0)==0x80)) {
				o[n++]=((c&0x07)<<18)|((c1&0x3F)<<12)|((c2&0x3F)<<6)|(c3&0x3F); i+=4;
			} else i++;
		} else i++;
	}
	return n;
}

// output buffer
static float *tts_output_buf = NULL;
static int    tts_output_len = 0;

// public api
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_sample_rate(void) { return SAMPLE_RATE; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_language(int lang)
{
	if (lang == 1)      current_lang = LANG_EN;
	else if (lang == 2) current_lang = LANG_AUTO;
	else                current_lang = LANG_RU;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_speed(double spd)
{ if (spd < 0.1) spd = 0.1; if (spd > 8.0) spd = 8.0; tts_read_speed = spd; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_pitch(double hz)
{ if (hz < 50.0) hz = 50.0; if (hz > 300.0) hz = 300.0; prosody_base_f0 = hz; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_speak(const char *txt)
{
	if (!txt) return 0;
	static int seeded = 0;
	if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }

	LangID eff = (current_lang == LANG_AUTO) ? detect_lang(txt) : current_lang;
	TTSSeq *s  = NULL;

	if (eff == LANG_EN) {
		char *exp = en_expand_input(txt);
		const char *use = exp ? exp : txt;
		uint32_t *codes = (uint32_t *)malloc((size_t)MAX_UTF8_CP * sizeof(uint32_t));
		if (!codes) { if (exp) free(exp); return 0; }
		int ni = utf8_to_cp(use, codes, MAX_UTF8_CP);
		if (exp) free(exp);
		if (ni > 0) s = prepare_sequence_en(codes, ni);
		free(codes);
	} else {
		char *exp = ru_expand_input(txt);
		const char *use = exp ? exp : txt;
		int half = MAX_UTF8_CP / 2;
		uint32_t *codes = (uint32_t *)malloc((size_t)half * sizeof(uint32_t));
		uint32_t *norm  = (uint32_t *)malloc((size_t)half * sizeof(uint32_t));
		if (!codes || !norm) { free(codes); free(norm); if (exp) free(exp); return 0; }
		int ncp = utf8_to_cp(use, codes, half);
		if (exp) free(exp);
		int ni = 0;
		for (int i = 0; i < ncp && ni < half; i++) norm[ni++] = ru_normalize_upper(codes[i]);
		free(codes);
		if (ni > 0) s = prepare_sequence_ru(norm, ni);
		free(norm);
	}

	if (!s) return 0;

	if (whisper_mode)
		whisper_transform_seq(s);

	long long total = 0;
	for (int i = 0; i < s->seqLen; i++) total += (long long)s->seq[i].totalSamples;
	if (total <= 0) { free_tts(s); return 0; }
	if (total > SAMPLE_RATE * 90) total = SAMPLE_RATE * 90;

	if (tts_output_buf) { free(tts_output_buf); tts_output_buf = NULL; tts_output_len = 0; }
	tts_output_buf = (float *)malloc((size_t)total * sizeof(float));
	if (!tts_output_buf) { free_tts(s); return 0; }

	reset_seq(s);
	int idx = 0;
	while (s->currentIndex < s->seqLen && idx < (int)total)
		tts_output_buf[idx++] = generate_sample(s);
	free_tts(s);

	tts_output_len = idx;
	postprocess(tts_output_buf, tts_output_len, SAMPLE_RATE);
	return tts_output_len;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
float *tts_get_buf(void) { return tts_output_buf; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_get_len(void) { return tts_output_len; }
