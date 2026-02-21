#include "tts_synth.h"
#include "lang_ru.h"
#include "lang_en.h"
#include <time.h>
#include <stdio.h>

/* language selection state */
typedef enum { LANG_RU = 0, LANG_EN = 1, LANG_AUTO = 2 } LangID;
static LangID current_lang = LANG_AUTO;

/* detect language by scanning utf-8 string for cyrillic vs ascii script */
static LangID detect_lang(const char *txt)
{
	int ru = 0, en = 0;
	const unsigned char *s = (const unsigned char *)txt;
	while (*s) {
		unsigned char c = *s;
		if (c < 0x80) {
			if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) en++;
			s++;
		} else if ((c & 0xE0) == 0xC0) {
			/* cyrillic falls within u+0400–u+04ff (0xd0 or 0xd1 in utf-8) */
			if (c == 0xD0 || c == 0xD1) ru++;
			s += 2;
		} else if ((c & 0xF0) == 0xE0) { s += 3; }
		else { s++; }
	}
	if (ru == 0 && en == 0) return LANG_RU;
	return (en > ru) ? LANG_EN : LANG_RU;
}

/* convert utf-8 string to array of 32-bit codepoints */
static int utf8_to_codepoints(const char *s, uint32_t *o, int max_out)
{
	int i = 0, n = 0;
	while (s[i] && n < max_out) {
		unsigned char c = (unsigned char)s[i];
		if      (c < 0x80)   { o[n++] = c; i += 1; }
		else if ((c & 0xE0) == 0xC0){ o[n++] = ((c&0x1F)<<6)|((unsigned char)s[i+1]&0x3F); i += 2; }
		else if ((c & 0xF0) == 0xE0){ o[n++] = ((c&0x0F)<<12)|(((unsigned char)s[i+1]&0x3F)<<6)|((unsigned char)s[i+2]&0x3F); i += 3; }
		else i++;
	}
	return n;
}

/* collapse consecutive identical phonemic characters */
typedef struct { uint32_t cp; int count; } RunEntry;

static int collapse_runs(const uint32_t *norm, int ni, RunEntry *runs)
{
	int ri = 0;
	for (int i = 0; i < ni; ) {
		uint32_t cp = norm[i];
		int cnt = 1;
		while (i + cnt < ni && norm[i + cnt] == cp) cnt++;
		runs[ri].cp = cp;
		runs[ri].count = cnt;
		ri++;
		i += cnt;
	}
	return ri;
}

/* prepare synthesis sequence for russian text */
static TTSSeq *prepare_sequence_ru(const uint32_t *norm, int ni)
{
	RunEntry *runs = malloc((ni + 4) * sizeof(RunEntry));
	if (!runs) return NULL;
	int nruns = collapse_runs(norm, ni, runs);

	FormantData *seq = calloc(nruns + 8, sizeof(FormantData));
	if (!seq) { free(runs); return NULL; }
	int seq_len = 0;

	for (int i = 0; i < nruns; i++) {
		uint32_t cp = runs[i].cp;
		int cnt = runs[i].count;

		double psec = ru_punctuation_pause(cp);
		if (psec > 0.0) {
			seq[seq_len].sampleRate = SAMPLE_RATE;
			seq[seq_len].totalSamples = (int)(psec * cnt * SAMPLE_RATE / READ_SPEED);
			if (seq[seq_len].totalSamples < 2) seq[seq_len].totalSamples = 2;
			seq[seq_len].type = vtype_silence;
			seq[seq_len].dbg_code = cp;
			seq_len++; continue;
		}

		if (cp == 0) continue;

		PhonemeDef *pd = NULL;
		for (int k = 0; ru_phonemes[k].code != 0; k++) {
			if (ru_phonemes[k].code == cp) { pd = &ru_phonemes[k]; break; }
		}

		if (!pd) {
			seq[seq_len].sampleRate = SAMPLE_RATE;
			seq[seq_len].totalSamples = (int)(0.04 * SAMPLE_RATE / READ_SPEED);
			seq[seq_len].type = vtype_silence;
			seq[seq_len].dbg_code = cp;
			seq_len++; continue;
		}

		if (pd->type == vtype_silence && cp == 0x042C) continue;

		double total_dur = pd->duration * cnt;
		setup_formant(&seq[seq_len], pd, cp, total_dur);
		seq_len++;
	}

	free(runs);
	if (seq_len == 0) { free(seq); return NULL; }

	TTSSeq *tts = malloc(sizeof(TTSSeq));
	tts->seq = seq;
	tts->seqLen = seq_len;
	tts->currentIndex = 0;
	return tts;
}

/* prepare synthesis sequence for english text using g2p and coarticulation */
static TTSSeq *prepare_sequence_en(const uint32_t *norm, int ni)
{
	FormantData *seq = calloc(ni * 4 + 16, sizeof(FormantData));
	if (!seq) return NULL;
	int seq_len = 0;

	char word_buf[512];
	int wlen = 0;
	uint32_t phones[512];

	/* flush word buffer, convert to phonemes, and apply coarticulation context */
	#define FLUSH_WORD() do { \
	if (wlen > 0) { \
		word_buf[wlen] = 0; \
		int nph = en_grapheme_to_phonemes(word_buf, phones, 512); \
		if (nph > 0) { \
			for (int pi = 0; pi < nph; ++pi) { \
				uint32_t cur_ph = phones[pi]; \
				uint32_t prev_ph = (pi > 0) ? phones[pi - 1] : 0; \
				uint32_t next_ph = (pi + 1 < nph) ? phones[pi + 1] : 0; \
				PhonemeDef tmp_pd; \
				en_coarticulate_context(prev_ph, cur_ph, next_ph, &tmp_pd); \
				setup_formant(&seq[seq_len], &tmp_pd, cur_ph, 0.0); \
				seq_len++; \
			} \
		} \
		wlen = 0; \
	} \
	} while(0)

	for (int i = 0; i < ni; i++) {
		uint32_t cp = norm[i];
		double psec = en_punctuation_pause(cp);

		if (psec > 0.0 || cp == 0) {
			FLUSH_WORD();
			if (psec > 0.0) {
				seq[seq_len].sampleRate = SAMPLE_RATE;
				seq[seq_len].totalSamples = (int)(psec * SAMPLE_RATE / READ_SPEED);
				if (seq[seq_len].totalSamples < 2) seq[seq_len].totalSamples = 2;
				seq[seq_len].type = vtype_silence;
				seq[seq_len].dbg_code = cp;
				seq_len++;
			}
			continue;
		}

		if (cp < 0x80) {
			char c = (char)cp;
			if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
			if ((c >= 'a' && c <= 'z') && wlen < (int)sizeof(word_buf) - 2)
				word_buf[wlen++] = c;
			else FLUSH_WORD();
		} else {
			FLUSH_WORD();
		}
	}
	FLUSH_WORD();
	#undef FLUSH_WORD

	if (seq_len == 0) { free(seq); return NULL; }

	TTSSeq *tts = malloc(sizeof(TTSSeq));
	tts->seq = seq;
	tts->seqLen = seq_len;
	tts->currentIndex = 0;
	return tts;
}

/* static output buffer management */
static float *tts_output_buf = NULL;
static int tts_output_len = 0;

/* emscripten public api */

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_sample_rate(void) { return SAMPLE_RATE; }

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_language(int lang)
{
	/* 0=ru, 1=en, 2=auto */
	if      (lang == 1) current_lang = LANG_EN;
	else if (lang == 2) current_lang = LANG_AUTO;
	else                current_lang = LANG_RU;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void tts_set_speed(double spd)
{
	if (spd < 0.1) spd = 0.1;
	if (spd > 8.0) spd = 8.0;
	READ_SPEED = spd;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int tts_speak(const char *txt)
{
	if (!txt) return 0;

	static int seeded = 0;
	if (!seeded) { srand((unsigned int)time(NULL)); seeded = 1; }

	LangID effective = (current_lang == LANG_AUTO) ? detect_lang(txt) : current_lang;
	TTSSeq *s = NULL;

	if (effective == LANG_EN) {
		char *expanded = en_expand_input(txt);
		const char *use = expanded ? expanded : txt;
		uint32_t codes[8192];
		int ni = utf8_to_codepoints(use, codes, 8192);
		if (expanded) free(expanded);
		if (ni <= 0) return 0;
		s = prepare_sequence_en(codes, ni);
	} else {
		char *expanded = ru_expand_input(txt);
		const char *use = expanded ? expanded : txt;
		uint32_t codes[4096], norm[4096];
		int ncp = utf8_to_codepoints(use, codes, 4096);
		if (expanded) free(expanded);
		if (ncp <= 0) return 0;
		int ni = 0;
		for (int i = 0; i < ncp; i++) norm[ni++] = ru_normalize_upper(codes[i]);
		s = prepare_sequence_ru(norm, ni);
	}

	if (!s) return 0;

	int total = 0;
	for (int i = 0; i < s->seqLen; i++) total += s->seq[i].totalSamples;

	if (tts_output_buf) free(tts_output_buf);
	tts_output_buf = malloc(total * sizeof(float));
	if (!tts_output_buf) { free_tts(s); return 0; }

	reset_seq(s);
	int idx = 0;
	while (s->currentIndex < s->seqLen && idx < total)
		tts_output_buf[idx++] = generate_sample(s);

	free_tts(s);
	tts_output_len = idx;
	return idx;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
float *tts_get_buf(void) { return tts_output_buf; }
