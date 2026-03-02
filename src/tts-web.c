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

// emotion ids neutral sad happy angry scared
typedef enum {
	EMOTION_NEUTRAL = 0,
	EMOTION_SAD     = 1,
	EMOTION_HAPPY   = 2,
	EMOTION_ANGRY   = 3,
	EMOTION_SCARED  = 4
} EmotionID;

static EmotionID current_emotion = EMOTION_NEUTRAL;

// emotion model - 3 layers: macro (utterance shape) meso (syllable) micro (frame quality)
// refs: Schroeder2001 Banse&Scherer1996 Yildirim2004 theater2025 Gangamohan

// macro params - global utterance shape per emotion
// f0_mean_shift_st  semitone shift of mean pitch vs neutral
// f0_range_mul      F0 excursion amplitude multiplier (1=neutral)
// speed_mul         utterance rate relative to user setting
// pause_mul         inter-word pause duration multiplier
// energy_mul        RMS amplitude (best single separator per Yildirim2004)
// onset_boost_st    semitones added to utterance-initial syllable
// final_fall_st     semitone drop on final nucleus
// accent_amp_mul    stressed syllable prominence above baseline
typedef struct {
	float f0_mean_shift_st;
	float f0_range_mul;
	float speed_mul;
	float pause_mul;
	float energy_mul;
	float onset_boost_st;
	float final_fall_st;
	float accent_amp_mul;
} EmoMacro;

// micro params - frame-level voice quality
// jitter_pct       cycle-to-cycle F0 variation [0..1]
// shimmer_pct      cycle-to-cycle amplitude variation [0..1]
// breathiness      noise injection into voiced frames [0..1]
// creaky_thr       creaky phonation threshold on low-energy frames
// vibrato_hz       vibrato carrier freq (0=off)
// vibrato_st       vibrato depth in semitones
// voice_break_prob probability per voiced-frame of transient pitch jump [0..1]
// voice_break_st   semitone size of voice break excursion
typedef struct {
	float jitter_pct;
	float shimmer_pct;
	float breathiness;
	float creaky_thr;
	float vibrato_hz;
	float vibrato_st;
	float voice_break_prob;
	float voice_break_st;
} EmoMicro;

// neutral baseline - everything is identity / off
static const EmoMacro MACRO_NEUTRAL = {
	0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, 2.0f, 1.0f
};
static const EmoMicro MICRO_NEUTRAL = {
	0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

static const EmoMacro MACRO_SAD = {
	-5.0f,   // pitch tanks hard
	0.35f,   // crushing narrow range - nearly monotone
	0.65f,   // painfully slow
	2.80f,   // long aching pauses between words
	0.62f,   // quiet deflated voice
	-2.5f,   // onset already below baseline - no energy to start
	9.0f,    // extreme final fall - utterance just dies
	0.45f,   // stress barely registers - flat affect
};
static const EmoMicro MICRO_SAD = {
	0.035f,  // slight jitter - unstable breathy voicing
	0.045f,  // shimmer - wavering energy
	0.32f,   // heavy breathiness - grief open glottis
	0.40f,   // creaky on low frames - voice giving out
	3.8f,    // slow quiver vibrato - trembling lip
	0.75f,   // deep tremolo - clearly audible shaking
	0.0012f, // occasional sorrow cracks
	2.5f,    // small break - quiet crack not a big jump
};

// happy - widest F0 range (~173 Hz theater2025) bouncy staircase lax clean voice
// Schroeder2001: F0 range +9st fast loud key diff from anger: laxer voice
static const EmoMacro MACRO_HAPPY = {
	+5.5f,   // noticeably higher pitch - sounds upbeat immediately
	2.80f,   // huge range - wild excitable swings
	1.30f,   // faster clipped delivery
	0.35f,   // short punchy pauses - no time to be sad
	1.28f,   // louder - projecting excitement
	+7.0f,   // massive onset burst - jumps in with energy
	0.8f,    // almost no final fall - ends on a high note
	2.20f,   // huge bouncy accent peaks
};
static const EmoMicro MICRO_HAPPY = {
	0.006f,  // clean lax voice - opposite of angry's harshness
	0.010f,  // minimal shimmer - steady bright voice
	0.0f,    // no breathiness - modal pressed voice
	0.0f,    // no creaky
	7.5f,    // fast shimmer-vibrato - excitement flutter
	0.35f,   // noticeable shimmer depth - sparkly quality
	0.0f,    // no breaks - nothing goes wrong when happy
	0.0f,
};

// angry - highest energy widest range tense pressed phonation irregular spikes
// key: diff from happy by HARSHNESS - high jitter + narrow formants not just pitch
static const EmoMacro MACRO_ANGRY = {
	+3.5f,   // higher pitch - raised larynx under tension
	3.20f,   // enormous range - huge swings between spikes and valleys
	1.45f,   // fast clipped - no time for full phonation
	0.20f,   // barely any pauses - just keeps attacking
	1.60f,   // loudest of all emotions
	+10.0f,  // explosive onset - punches in at maximum force
	6.0f,    // hard brutal final drop - phrase cut with a knife
	2.80f,   // huge accent spikes - every stress is a blow
};
static const EmoMicro MICRO_ANGRY = {
	0.110f,  // very high jitter - tense pressed glottis (main timbre marker)
	0.085f,  // high shimmer - irregular glottal pulses from tension
	0.0f,    // no breathiness - pressed glottis is the opposite
	0.0f,    // no creaky - too much energy for that
	0.0f,    // no smooth vibrato - replaced by irregular rage bursts
	0.0f,
	0.025f,  // frequent stress spikes - rage micro-bursts
	+6.0f,   // big upward spike then falls - like a shout peak
};

// scared - highest jitter of all (Banse&Scherer1996) unstable flutter + voice breaks
// paradox: high F0 floor even in pauses can't keep pitch down panic tempo
static const EmoMacro MACRO_SCARED = {
	+6.0f,   // pitch shoots high - fight-or-flight larynx elevation
	1.25f,   // medium range but totally erratic within it
	1.50f,   // panic speed - hyperventilating
	0.30f,   // tiny pauses - no time to breathe
	0.78f,   // quieter - inhibited constricted voice
	+8.5f,   // pitch surges on very first syllable - shock reflex
	4.0f,    // falls off at end - running out of air
	1.50f,   // accents smeared by jitter - can't control voice
};
static const EmoMicro MICRO_SCARED = {
	0.185f,  // EXTREME jitter - most unstable voice of any emotion
	0.120f,  // heavy shimmer - shaking body = shaking voice
	0.38f,   // breathy AND jittery - open+irregular glottis simultaneously
	0.12f,   // creaky on low frames - voice barely holding together
	11.0f,   // very fast tremolo - whole body vibrating
	0.50f,   // deep tremolo - audibly shaking
	0.050f,  // very frequent voice breaks - single most recognizable fear cue
	+8.0f,   // large break - sudden octave jump then crash
};

// semitone to frequency ratio
static inline float st_to_ratio(float st)
{
	return powf(2.0f, st / 12.0f);
}

// layer 1 - macro f0 contour for normalised utterance position t [0..1]
// dispatches by emotion params into per-emotion contour shapes
static float macro_f0(float base_f0, const EmoMacro *em,
					  float t, int i, int n,
					  int is_stressed, int is_question)
{
	// mean pitch after semitone shift
	float mean_f0 = base_f0 * st_to_ratio(em->f0_mean_shift_st);

	// range in Hz: proportional to mean and range multiplier
	float range_hz = mean_f0 * 0.55f * em->f0_range_mul;

	float contour = 0.0f;

	// sad - steep linear drop the whole way + cliff edge in last 15%
	// no onset energy starts below baseline and just falls
	if (em->final_fall_st > 7.0f && em->f0_range_mul < 0.5f) {
		contour = -range_hz * 0.45f * t;
		if (t > 0.82f)
			contour -= range_hz * powf((t - 0.82f) / 0.18f, 1.5f) * 0.55f;
	}

	// happy - staircase: each syllable zone arches up then next starts higher
	// overall rising trend first half exclamatory peak at end
	else if (em->onset_boost_st > 4.0f && em->f0_range_mul > 1.8f && em->energy_mul > 1.0f) {
		float step_phase = fmodf(t * 8.0f, 1.0f);
		float bounce = sinf(step_phase * (float)M_PI) * range_hz * 0.30f;
		float stair  = t * range_hz * 0.18f;  // each step starts a bit higher
		float peak   = (t > 0.80f) ? (t - 0.80f) / 0.20f * range_hz * 0.25f : 0.0f;
		contour = bounce + stair + peak;
	}

	// angry - explosive onset massive plateau punch spikes every ~14% brutal cliff end
	else if (em->accent_amp_mul > 2.0f && em->energy_mul > 1.3f) {
		float spike_phase = fmodf(t * 7.5f, 1.0f);
		// each spike is a sharp sawtooth - up fast then falls
		float spike = (spike_phase < 0.18f)
		? (1.0f - spike_phase / 0.18f) * range_hz * 0.55f
		: 0.0f;
		// envelope: detonates at start high plateau then snaps off
		float env = (t < 0.10f) ? range_hz * (1.0f - t / 0.10f) * 0.60f
		: (t > 0.78f) ? -range_hz * powf((t - 0.78f) / 0.22f, 0.7f) * 0.70f
		:               range_hz * 0.05f;  // slight high plateau
		contour = spike + env;
	}

	// scared - panic surge onset then three-frequency flutter chaos + voice break gaps
	else if (em->onset_boost_st > 5.0f && em->f0_range_mul < 1.5f) {
		// three superimposed oscillators at inharmonic ratios = uncontrolled trembling
		float flutter = sinf(t * (float)M_PI * 19.0f) * range_hz * 0.22f
		+ sinf(t * (float)M_PI * 13.7f + 1.1f) * range_hz * 0.15f
		+ sinf(t * (float)M_PI *  7.3f + 2.4f) * range_hz * 0.09f;
		// panic surge: pitch rockets up at start then just barely holds
		float panic = (t < 0.20f) ? range_hz * 0.45f * (1.0f - t / 0.20f) : 0.0f;
		// slight downward drift as adrenaline runs out
		float drift = -range_hz * 0.12f * t;
		contour = flutter + panic + drift;
	}

	float f0 = mean_f0 + contour;

	// onset boost - first syllable gets sharp lift sets attack character immediately
	if (i == 0 && em->onset_boost_st != 0.0f) {
		f0 *= st_to_ratio(em->onset_boost_st * 0.75f);
	} else if (i == 1 && em->onset_boost_st > 0.0f) {
		f0 *= st_to_ratio(em->onset_boost_st * 0.35f);  // echo of onset
	}

	// final fall on last two syllables
	if (i >= n - 2 && em->final_fall_st > 0.0f) {
		float ff = (float)(n - 1 - i);
		f0 /= st_to_ratio(em->final_fall_st * (1.0f - ff * 0.5f));
	}

	// stressed syllable rides above curve
	if (is_stressed) {
		f0 += range_hz * 0.22f * (em->accent_amp_mul - 1.0f);
	}

	// question rise - not for angry it just cuts off harder
	if (is_question && em->final_fall_st < 7.0f && t > 0.82f) {
		float qt = (t - 0.82f) / 0.18f;
		f0 += qt * mean_f0 * 0.22f;
	}

	if (f0 < 50.0f)  f0 = 50.0f;
	if (f0 > 600.0f) f0 = 600.0f;
	return f0;
}

// layer 3 - micro modulation applied per voiced frame on top of macro shape

typedef struct {
	int    frames_since_break;
	float  break_carry;
	float  vibrato_phase;
} MicroState;

static MicroState g_micro_state = {0, 0.0f, 0.0f};

static float micro_f0(float f0, const EmoMicro *em, float frame_dt)
{
	float out = f0;

	// jitter - random cycle-to-cycle variation simulates glottal irregularity
	if (em->jitter_pct > 0.0f) {
		float j = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;
		out *= (1.0f + j * em->jitter_pct);
	}

	// vibrato - smooth sinusoidal modulation sad=tremolo happy=shimmer scared=flutter
	if (em->vibrato_hz > 0.0f && em->vibrato_st > 0.0f) {
		g_micro_state.vibrato_phase += em->vibrato_hz * frame_dt * 2.0f * (float)M_PI;
		float vib   = sinf(g_micro_state.vibrato_phase);
		float ratio = powf(2.0f, em->vibrato_st * vib / 12.0f);
		out *= ratio;
	}

	// voice breaks - sudden pitch excursion then fast exponential recovery
	// minimum gap enforced so breaks don't cluster unnaturally
	g_micro_state.frames_since_break++;
	if (em->voice_break_prob > 0.0f && em->voice_break_st != 0.0f) {
		int min_gap = (int)(0.08f / frame_dt);
		if (g_micro_state.frames_since_break > min_gap) {
			float r = (float)rand() / (float)RAND_MAX;
			if (r < em->voice_break_prob) {
				g_micro_state.break_carry = em->voice_break_st;
				g_micro_state.frames_since_break = 0;
			}
		}
	}
	if (fabsf(g_micro_state.break_carry) > 0.05f) {
		out *= st_to_ratio(g_micro_state.break_carry);
		g_micro_state.break_carry *= 0.28f;  // sharp break fast return
	}

	if (out < 50.0f)  out = 50.0f;
	if (out > 600.0f) out = 600.0f;
	return out;
}

static float micro_amp(float amp, const EmoMicro *em)
{
	if (em->shimmer_pct <= 0.0f) return amp;
	float s = ((float)rand() / (float)RAND_MAX - 0.5f) * 2.0f;
	return amp * (1.0f + s * em->shimmer_pct);
}

static void get_emo_params(EmotionID eid,
						   const EmoMacro **m, const EmoMicro **u)
{
	static const EmoMacro *macro_table[5];
	static const EmoMicro *micro_table[5];
	static int init = 0;
	if (!init) {
		macro_table[EMOTION_NEUTRAL] = &MACRO_NEUTRAL;
		macro_table[EMOTION_SAD]     = &MACRO_SAD;
		macro_table[EMOTION_HAPPY]   = &MACRO_HAPPY;
		macro_table[EMOTION_ANGRY]   = &MACRO_ANGRY;
		macro_table[EMOTION_SCARED]  = &MACRO_SCARED;
		micro_table[EMOTION_NEUTRAL] = &MICRO_NEUTRAL;
		micro_table[EMOTION_SAD]     = &MICRO_SAD;
		micro_table[EMOTION_HAPPY]   = &MICRO_HAPPY;
		micro_table[EMOTION_ANGRY]   = &MICRO_ANGRY;
		micro_table[EMOTION_SCARED]  = &MICRO_SCARED;
		init = 1;
	}
	*m = macro_table[eid];
	*u = micro_table[eid];
}

// voice quality - formant bandwidth modification
// breathy: widens formant bandwidths open glottis low Q
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

// breathy voice - widen formant bandwidths reduce amplitude open glottis model
static void apply_breathiness(FormantData *fd, float breathiness)
{
	if (!fd || breathiness <= 0.0f) return;
	if (fd->type != vtype_vowel && fd->type != vtype_consonant) return;
	if (!fd->is_voiced) return;

	fd->amplitude *= (1.0f - breathiness * 0.40f);

	for (int k = 0; k < 3; k++) {
		double fc = fd->f[k];
		if (fc < 80.0) continue;
		double Q     = 9.0 - breathiness * 6.0;
		if (Q < 1.2) Q = 1.2;
		double w0    = TWO_PI * fc / SAMPLE_RATE;
		double alpha = sin(w0) / (2.0 * Q);
		double cosw0 = cos(w0);
		double a0    = 1.0 + alpha;
		fd->b0[k] =  alpha / a0;
		fd->b1[k] =  0.0;
		fd->b2[k] = -alpha / a0;
		fd->a1[k] = -2.0 * cosw0 / a0;
		fd->a2[k] = (1.0 - alpha) / a0;
	}
}

// tense pressed voice (angry) - narrow formant bandwidths high Q
// simulates glottal adduction and increased subglottal pressure
static void apply_tense_voice(FormantData *fd, float tension)
{
	if (!fd || tension <= 0.0f) return;
	if (fd->type != vtype_vowel) return;
	if (!fd->is_voiced) return;

	fd->amplitude = fminf(fd->amplitude * (1.0f + tension * 0.25f), 1.0f);

	for (int k = 0; k < 3; k++) {
		double fc = fd->f[k];
		if (fc < 80.0) continue;
		double Q     = 6.0 + tension * 8.0;
		if (Q > 18.0) Q = 18.0;
		double w0    = TWO_PI * fc / SAMPLE_RATE;
		double alpha = sin(w0) / (2.0 * Q);
		double cosw0 = cos(w0);
		double a0    = 1.0 + alpha;
		fd->b0[k] =  alpha / a0;
		fd->b1[k] =  0.0;
		fd->b2[k] = -alpha / a0;
		fd->a1[k] = -2.0 * cosw0 / a0;
		fd->a2[k] = (1.0 - alpha) / a0;
	}
}

// emotion_transform_seq - post-processing pass applies macro timing/amplitude and micro modulation
// macro f0 already baked per-phoneme during sequence build via compute_prosody
static void emotion_transform_seq(TTSSeq *seq, EmotionID eid)
{
	if (!seq || !seq->seq || seq->seqLen <= 0) return;
	if (eid == EMOTION_NEUTRAL) return;

	const EmoMacro *em;
	const EmoMicro *eu;
	get_emo_params(eid, &em, &eu);

	memset(&g_micro_state, 0, sizeof(g_micro_state));

	int nv = 0;
	for (int i = 0; i < seq->seqLen; i++)
		if (seq->seq[i].type != vtype_silence) nv++;

		int vi = 0;
	for (int i = 0; i < seq->seqLen; i++) {
		FormantData *fd = &seq->seq[i];

		// silence frames - scale pauses per emotion character
		if (fd->type == vtype_silence) {
			double ps = fd->totalSamples / (double)SAMPLE_RATE;
			ps *= em->pause_mul / em->speed_mul;
			if (eid == EMOTION_ANGRY && ps > 0.06) ps = 0.06;
			if (eid == EMOTION_SAD   && ps < 0.12) ps = 0.12;
			int ts = (int)ceil(ps * SAMPLE_RATE);
			if (ts < 2) ts = 2;
			fd->totalSamples = ts;
			continue;
		}

		float t = (nv > 1) ? (float)vi / (float)(nv - 1) : 0.0f;
		vi++;

		// duration scaling
		double dur = fd->totalSamples / (double)SAMPLE_RATE;
		dur /= em->speed_mul;

		switch (eid) {
			case EMOTION_SAD:
				if (fd->type == vtype_vowel)     dur *= 1.45;  // drawn-out like dragging feet
				if (fd->type == vtype_fricative) dur *= 1.30;  // slow sighing sss
				break;
			case EMOTION_HAPPY:
				if (fd->type == vtype_vowel)     dur *= 0.85;  // clipped energetic vowels
				if (fd->type == vtype_stop)      dur *= 0.75;  // sharp crisp pops
				break;
			case EMOTION_ANGRY:
				if (fd->type == vtype_consonant) dur *= 0.65;  // hard clipped consonants
				if (fd->type == vtype_vowel)     dur *= 0.78;  // truncated - no time for vowels
				if (vi == nv)                    dur *= 0.50;  // last vowel cut brutally short
				break;
			case EMOTION_SCARED:
				if (fd->type == vtype_fricative) dur *= 0.55;  // choppy uneven fricatives
				if (fd->type == vtype_vowel)     dur *= 0.75;  // fast vowels - hyperventilating
				break;
			default: break;
		}

		int ts = (int)ceil(dur * SAMPLE_RATE);
		if (ts < 2) ts = 2;
		fd->totalSamples = ts;

		// amplitude
		fd->amplitude *= em->energy_mul;
		if (eu->shimmer_pct > 0.0f && fd->is_voiced)
			fd->amplitude = micro_amp(fd->amplitude, eu);
		if (fd->amplitude > 1.0f) fd->amplitude = 1.0f;
		if (fd->amplitude < 0.0f) fd->amplitude = 0.0f;

		// voice quality - formant mods per emotion
		if (eid == EMOTION_ANGRY && fd->type == vtype_vowel) {
			apply_tense_voice(fd, 0.90f);    // max press - screaming glottis
			fd->f[0] = (int)(fd->f[0] * 1.12f);  // raised f1 wide open jaw during shout
			if (fd->f[0] > 980) fd->f[0] = 980;
		}
		if (eid == EMOTION_SAD && fd->type == vtype_vowel) {
			fd->f[0] = (int)(fd->f[0] * 0.88f);  // droopy f1 depressed jaw
			fd->f[1] = (int)(fd->f[1] * 0.93f);  // retracted f2 backed tongue
			apply_breathiness(fd, eu->breathiness * (0.4f + t * 0.6f));  // gets breathier toward end
		}
		if (eid == EMOTION_HAPPY && fd->type == vtype_vowel) {
			fd->f[1] = (int)(fd->f[1] * 1.07f);  // fronted f2 spread lips smiling
			fd->f[2] = (int)(fd->f[2] * 1.04f);  // slightly raised f3 bright timbre
		}
		if (eid == EMOTION_SCARED && fd->type == vtype_vowel) {
			fd->f[0] = (int)(fd->f[0] * 1.18f);  // raised f1 hyperarticulated stress
			fd->f[1] = (int)(fd->f[1] * 1.10f);  // fronted f2 tense body
			apply_breathiness(fd, eu->breathiness * (0.6f + t * 0.4f));
		}

		// F0 micro-modulation
		if (fd->is_voiced) {
			float base_f0 = (float)fd->dbg_code;
			if (base_f0 < 50.0f) base_f0 = (float)prosody_base_f0;
			float frame_dt = (float)(fd->totalSamples) / (float)SAMPLE_RATE;
			fd->dbg_code = (uint32_t)roundf(micro_f0(base_f0, eu, frame_dt));
		}
	}
}

// whisper transform - devoices everything converts vowels to noise-sourced formants
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

// postprocess normalise -> lp -> dc block -> soft tanh limiter with per-emotion drive
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

	float peak = 0.0f;
	for (int i = 0; i < n; i++) { float a = fabsf(buf[i]); if (a > peak) peak = a; }
	if (peak > 1e-6f) {
		float s = 0.75f / peak;
		for (int i = 0; i < n; i++) buf[i] *= s;
	}

	LPF2 lp1, lp2;
	lpf2_init(&lp1, 7500.f, sr);
	lpf2_init(&lp2, 7500.f, sr);

	DCB dc = {0, 0};

	// tanh soft limiter drive controls warmth/aggression per emotion
	float drive = 1.4f;
	if (current_emotion == EMOTION_ANGRY)  drive = 2.8f;  // hard aggressive overdrive - shouting grit
	if (current_emotion == EMOTION_SCARED) drive = 1.9f;  // slight crunch - tension in throat
	if (current_emotion == EMOTION_SAD)    drive = 1.0f;  // clean soft no saturation
	float inv_drive = 1.0f / drive;

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

// stop burst model - closure voicing bar vot aspiration burst transient
typedef struct {
	float  clos_ms;
	float  asp_ms;
	int    locus_f2;
	int    burst_f1;
	int    burst_f2;
	int    is_voiced;
} StopInfo;

static const StopInfo STOP_INFO[6] = {
	{55, 55,  800,  600, 1800, 0},   // p
	{55,  0,  800,  600, 1800, 1},   // b
	{62, 70, 1800, 2800, 5000, 0},   // t
	{58,  0, 1800, 2800, 5000, 1},   // d
	{70, 80, 2200, 1400, 3500, 0},   // k
	{65,  0, 2200, 1400, 3500, 1},   // g
};

static int stop_index(uint32_t code)
{
	if (code >= 0xE020u && code <= 0xE025u) return (int)(code - 0xE020u);
	return -1;
}

static int affricate_index(uint32_t code)
{
	if (code == EN_CH) return 0;
	if (code == EN_JH) return 1;
	return -1;
}

// expand_phone - convert single phoneme into one or more formant frames
// handles stops affricates fricatives vowels sonorants and fallback
static void expand_phone(
	FormantData *seq, int *idx, int seq_cap,
	uint32_t code, const PhonemeDef *pd,
	const PhonemeDef *prev_pd, const PhonemeDef *next_pd,
	double dur_scale, double amp_scale, uint32_t f0_hz)
{
	if (*idx >= seq_cap - 16) return;
	double spd = tts_read_speed;

	int si = stop_index(code);
	if (si >= 0) {
		float clos = (float)(STOP_INFO[si].clos_ms * dur_scale / spd);
		float asp  = (float)(STOP_INFO[si].asp_ms  * dur_scale / spd);
		int   lf2  = STOP_INFO[si].locus_f2;
		int   isvd = STOP_INFO[si].is_voiced;

		if (clos < MIN_FRAME_DUR * 1000.f) clos = MIN_FRAME_DUR * 1000.f;

		if (isvd) {
			PhonemeDef vb; memset(&vb, 0, sizeof(vb));
			vb.code = code; vb.f1 = 160; vb.f2 = lf2; vb.f3 = 2400;
			vb.duration = clos * 0.001; vb.type = vtype_consonant;
			vb.amp = 0.14f; vb.is_voiced = 1;
			push_frame(seq, idx, &vb, code, vb.duration, f0_hz);
		} else {
			PhonemeDef cl; memset(&cl, 0, sizeof(cl));
			cl.duration = clos * 0.001; cl.type = vtype_silence; cl.amp = 0.0f;
			push_frame(seq, idx, &cl, code, cl.duration, f0_hz);

			if (asp > 0.0f) {
				if (asp < MIN_FRAME_DUR * 1000.f) asp = MIN_FRAME_DUR * 1000.f;
				PhonemeDef ap; memset(&ap, 0, sizeof(ap));
				ap.code = EN_HH;
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

		PhonemeDef bst; memset(&bst, 0, sizeof(bst));
		bst.code = code;
		bst.f1   = STOP_INFO[si].burst_f1;
		bst.f2   = STOP_INFO[si].burst_f2;
		bst.f3   = 0;
		bst.duration  = 0.009 / spd;
		if (bst.duration < MIN_FRAME_DUR) bst.duration = MIN_FRAME_DUR;
		bst.type      = vtype_fricative;
		bst.is_voiced = isvd;
		bst.amp       = (float)(pd->amp * amp_scale * 1.10);
		push_frame(seq, idx, &bst, code, bst.duration, f0_hz);
		return;
	}

	int ai = affricate_index(code);
	if (ai >= 0) {
		int is_voiced = (code == EN_JH);
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
		double fr_dur = 0.080 * dur_scale / spd;
		if (fr_dur < MIN_FRAME_DUR) fr_dur = MIN_FRAME_DUR;
		PhonemeDef fr = *pd;
		fr.f1 = 1800; fr.f2 = 3500; fr.duration = fr_dur;
		fr.type = vtype_fricative;
		fr.amp  = (float)(pd->amp * amp_scale);
		push_frame(seq, idx, &fr, code, fr.duration, f0_hz);
		return;
	}

	if (pd->type == vtype_fricative) {
		double total = pd->duration * dur_scale / spd;
		double ramp  = 0.012 / spd;
		if (ramp < MIN_FRAME_DUR) ramp = MIN_FRAME_DUR;
		if (total < ramp * 3)  total = ramp * 3;
		double body  = total - ramp * 2;
		if (body < MIN_FRAME_DUR) body = MIN_FRAME_DUR;

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

	if (pd->type == vtype_vowel || pd->type == vtype_consonant) {
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
			interp_frame(seq, idx, &locus, pd, 0.6, tdur, f0_hz);
		}

		double dur = pd->duration * dur_scale / spd;
		if (dur < 0.030) dur = 0.030;
		PhonemeDef main_pd = *pd;
		main_pd.amp = (float)(pd->amp * amp_scale);
		main_pd.duration = dur;
		push_frame(seq, idx, &main_pd, code, dur, f0_hz);

		if (en_is_diphthong_onset(code)) {
			// glide phoneme follows in phones[]
		}
		return;
	}

	double dur = pd->duration * dur_scale / spd;
	if (dur < MIN_FRAME_DUR) dur = MIN_FRAME_DUR;
	PhonemeDef fallback = *pd;
	fallback.amp = (float)(pd->amp * amp_scale);
	push_frame(seq, idx, &fallback, code, dur, f0_hz);
}

// stress assignment simplified trochee/iamb rules based on espeak-ng cmu
static int find_stress(const uint32_t *phones, int n)
{
	int vpos[64]; int nv = 0;
	for (int i = 0; i < n && nv < 64; i++)
		if (en_is_vowel(phones[i]) && phones[i] != EN_AX &&
			phones[i] != EN_EY2 && phones[i] != EN_AY2 &&
			phones[i] != EN_AW2 && phones[i] != EN_OW2 && phones[i] != EN_OI2)
			vpos[nv++] = i;

		if (nv == 0) {
			for (int i = 0; i < n; i++) { PhonemeDef *p = en_find_phoneme(phones[i]); if (p && p->type==vtype_vowel) return i; }
			return 0;
		}
		if (nv == 1) return vpos[0];
		if (nv == 2) {
			uint32_t lv = phones[vpos[nv-1]];
			if (lv == EN_ER || lv == EN_IH || lv == EN_AX) return vpos[0];
			return vpos[nv-1];
		}
		int stress_v = nv - 2;
		uint32_t lv = phones[vpos[nv-1]];
		if (lv == EN_EY || lv == EN_AY || lv == EN_OW || lv == EN_AO || lv == EN_IY)
			stress_v = nv - 1;
	if (stress_v < 0) stress_v = 0;
	return vpos[stress_v];
}

// prosody model bakes macro f0 per-phoneme so emotion shape is in sequence from the start
typedef struct { float f0; float dur_scale; float amp_scale; } Prosody;

static void compute_prosody(const uint32_t *phones, int n,
							int stress_idx, int is_question,
							float base_f0, Prosody *out)
{
	const EmoMacro *em;
	const EmoMicro *eu;
	get_emo_params(current_emotion, &em, &eu);

	for (int i = 0; i < n; i++) {
		float t = (n > 1) ? (float)i / (float)(n - 1) : 0.f;
		int   is_stressed = (i == stress_idx);

		float f0 = macro_f0(base_f0, em, t, i, n, is_stressed, is_question);

		out[i].dur_scale = 1.0f;
		out[i].amp_scale = 1.0f;

		if (is_stressed) {
			out[i].dur_scale = 1.35f;
			out[i].amp_scale = 1.18f;
		} else {
			PhonemeDef *p = en_find_phoneme(phones[i]);
			if (p && p->type == vtype_vowel && phones[i] != EN_AX) {
				out[i].dur_scale = 0.78f;
				out[i].amp_scale = 0.88f;
			}
		}

		if (phones[i] == EN_AX) {
			out[i].dur_scale = 0.60f;
			out[i].amp_scale = 0.72f;
		}

		if (i == n-1 || i == n-2) {
			PhonemeDef *p = en_find_phoneme(phones[i]);
			if (p && p->type == vtype_vowel)
				out[i].dur_scale *= 1.15f;
		}

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

// language detection simple ru/en counters using utf8 byte patterns
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

// set emotion: 0 neutral 1 sad 2 happy 3 angry 4 scared
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_emotion(int emo)
{
	if      (emo == 1) current_emotion = EMOTION_SAD;
	else if (emo == 2) current_emotion = EMOTION_HAPPY;
	else if (emo == 3) current_emotion = EMOTION_ANGRY;
	else if (emo == 4) current_emotion = EMOTION_SCARED;
	else               current_emotion = EMOTION_NEUTRAL;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_whisper(int enable)
{
	whisper_mode = (enable != 0) ? 1 : 0;
}

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

	// whisper overrides emotion entirely
	if (whisper_mode) {
		whisper_transform_seq(s);
	} else if (current_emotion != EMOTION_NEUTRAL) {
		emotion_transform_seq(s, current_emotion);
	}

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
