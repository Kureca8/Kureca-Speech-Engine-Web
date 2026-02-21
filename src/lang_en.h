#pragma once

/* english phoneme table, grapheme-to-phoneme rules, and coarticulation
 * architecture and symbols preserved for compatibility.
 * coarticulation handles formant blending and duration tweaks.
 */

#include "tts_synth.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* internal phoneme ids (unicode private-use area) */

/* vowels */
#define EN_AE   0xE000u  /* ae: cat */
#define EN_AA   0xE001u  /* aa: car */
#define EN_AH   0xE002u  /* ah: cup */
#define EN_AO   0xE003u  /* ao: thought */
#define EN_AW   0xE004u  /* aw: out */
#define EN_EH   0xE005u  /* eh: bed */
#define EN_ER   0xE006u  /* er: bird */
#define EN_EY   0xE007u  /* ey: face */
#define EN_IH   0xE008u  /* ih: bit */
#define EN_IY   0xE009u  /* iy: fleece */
#define EN_OW   0xE00Au  /* ow: goat */
#define EN_UH   0xE00Bu  /* uh: foot */
#define EN_UW   0xE00Cu  /* uw: goose */
#define EN_AX   0xE00Du  /* ax: schwa */

/* stops */
#define EN_P    0xE010u
#define EN_B    0xE011u
#define EN_T    0xE012u
#define EN_D    0xE013u
#define EN_K    0xE014u
#define EN_G    0xE015u

/* fricatives */
#define EN_F    0xE020u
#define EN_V    0xE021u
#define EN_TH   0xE022u  /* th: thin */
#define EN_DH   0xE023u  /* dh: this */
#define EN_S    0xE024u
#define EN_Z    0xE025u
#define EN_SH   0xE026u
#define EN_ZH   0xE027u
#define EN_HH   0xE028u

/* affricates */
#define EN_CH   0xE030u
#define EN_JH   0xE031u

/* nasals and approximants */
#define EN_M    0xE040u
#define EN_N    0xE041u
#define EN_NG   0xE042u
#define EN_L    0xE043u
#define EN_R    0xE044u
#define EN_W    0xE045u
#define EN_Y    0xE046u

/* acoustic parameters: { code, f1, f2, f3, duration, type, amp, is_voiced } */
static PhonemeDef en_phonemes[] = {

    /* vowels: peterson & barney male averages adjusted for synth */
    {EN_AE,  660, 1720, 2410, 0.16, vtype_vowel,     1.05, 1},
    {EN_AA,  730, 1090, 2440, 0.18, vtype_vowel,     1.05, 1},
    {EN_AH,  640, 1190, 2390, 0.15, vtype_vowel,     1.02, 1},
    {EN_AO,  570,  840, 2410, 0.16, vtype_vowel,     1.02, 1},
    {EN_AW,  640, 1000, 2350, 0.15, vtype_vowel,     1.00, 1},
    {EN_EH,  580, 1800, 2605, 0.15, vtype_vowel,     1.02, 1},
    {EN_ER,  500, 1350, 1690, 0.17, vtype_vowel,     1.00, 1},
    {EN_EY,  440, 1900, 2550, 0.16, vtype_vowel,     1.03, 1},
    {EN_IH,  390, 1990, 2550, 0.14, vtype_vowel,     1.00, 1},
    {EN_IY,  270, 2290, 3010, 0.15, vtype_vowel,     1.00, 1},
    {EN_OW,  450,  900, 2250, 0.16, vtype_vowel,     1.03, 1},
    {EN_UH,  440, 1020, 2240, 0.14, vtype_vowel,     1.00, 1},
    {EN_UW,  300,  870, 2240, 0.16, vtype_vowel,     1.02, 1},
    {EN_AX,  500, 1500, 2400, 0.09, vtype_vowel,     0.90, 1},

    /* voiced stops */
    {EN_B,   250,  700,    0, 0.11, vtype_consonant, 0.60, 1},
    {EN_D,   250,  500,    0, 0.11, vtype_consonant, 0.58, 1},
    {EN_G,   200,  600,    0, 0.11, vtype_consonant, 0.56, 1},

    /* voiceless stops */
    {EN_P,   200,  600,    0, 0.10, vtype_stop,      0.48, 0},
    {EN_T,   300,  900,    0, 0.10, vtype_stop,      0.48, 0},
    {EN_K,   800, 1500,    0, 0.10, vtype_stop,      0.45, 0},

    /* fricatives */
    {EN_F,  2200, 4000,    0, 0.13, vtype_fricative, 0.32, 0},
    {EN_V,  2200, 4000,    0, 0.13, vtype_fricative, 0.34, 1},
    {EN_TH, 1500, 5500,    0, 0.12, vtype_fricative, 0.25, 0},
    {EN_DH, 1500, 5500,    0, 0.12, vtype_fricative, 0.28, 1},
    {EN_S,  4000, 6000,    0, 0.15, vtype_fricative, 0.33, 0},
    {EN_Z,  3500, 5500,    0, 0.15, vtype_fricative, 0.35, 1},
    {EN_SH, 2800, 4000,    0, 0.15, vtype_fricative, 0.36, 0},
    {EN_ZH, 2800, 4000,    0, 0.14, vtype_fricative, 0.36, 1},
    {EN_HH, 1000, 3000,    0, 0.11, vtype_fricative, 0.22, 0},

    /* affricates */
    {EN_CH, 2200, 4200,    0, 0.13, vtype_stop,      0.40, 0},
    {EN_JH, 2200, 4000,    0, 0.13, vtype_stop,      0.40, 1},

    /* nasals and approximants */
    {EN_M,   300,  900,    0, 0.13, vtype_consonant, 0.62, 1},
    {EN_N,   300, 1000,    0, 0.13, vtype_consonant, 0.62, 1},
    {EN_NG,  250,  700,    0, 0.12, vtype_consonant, 0.58, 1},
    {EN_L,   400,  900,    0, 0.13, vtype_consonant, 0.58, 1},
    {EN_R,   400, 1100,    0, 0.12, vtype_consonant, 0.55, 1},
    {EN_W,   300,  800,    0, 0.11, vtype_consonant, 0.53, 1},
    {EN_Y,   300, 2000,    0, 0.10, vtype_consonant, 0.50, 1},

    /* мёртвый */
    {0, 0, 0, 0, 0, 0, 0, 0}
};

/* g2p rules mapping patterns to phoneme sequences */
#define G2P_MAX_PHONES 4
typedef struct {
    const char *grapheme;
    uint32_t    phones[G2P_MAX_PHONES];
} G2PRule;

static G2PRule g2p_rules[] = {
    {"tion",  {EN_SH, EN_AX, EN_N, 0}},
    {"sion",  {EN_ZH, EN_AX, EN_N, 0}},
    {"ight",  {EN_EY, EN_T,  0,    0}},
    {"ough",  {EN_AW, 0,     0,    0}},
    {"eous",  {EN_UW, EN_AX, EN_S, 0}},
    {"tch",   {EN_CH, 0,     0,    0}},
    {"igh",   {EN_EY, 0,     0,    0}},
    {"ght",   {EN_T,  0,     0,    0}},
    {"ous",   {EN_AX, EN_S,  0,    0}},
    {"eau",   {EN_OW, 0,     0,    0}},
    {"whe",   {EN_W,  EN_EH, 0,    0}},
    {"ear",   {EN_IY, EN_R,  0,    0}},
    {"ire",   {EN_EY, EN_R,  0,    0}},
    {"ion",   {EN_EY, EN_AX, EN_N, 0}},
    {"ch",    {EN_CH, 0,     0,    0}},
    {"sh",    {EN_SH, 0,     0,    0}},
    {"th",    {EN_TH, 0,     0,    0}},
    {"ph",    {EN_F,  0,     0,    0}},
    {"wh",    {EN_W,  0,     0,    0}},
    {"ck",    {EN_K,  0,     0,    0}},
    {"ng",    {EN_NG, 0,     0,    0}},
    {"nk",    {EN_NG, EN_K,  0,    0}},
    {"gn",    {EN_N,  0,     0,    0}},
    {"kn",    {EN_N,  0,     0,    0}},
    {"wr",    {EN_R,  0,     0,    0}},
    {"dg",    {EN_JH, 0,     0,    0}},
    {"gh",    {0,     0,     0,    0}},
    {"qu",    {EN_K,  EN_W,  0,    0}},
    {"sc",    {EN_S,  0,     0,    0}},
    {"aw",    {EN_AO, 0,     0,    0}},
    {"ow",    {EN_OW, 0,     0,    0}},
    {"oo",    {EN_UW, 0,     0,    0}},
    {"ou",    {EN_AW, 0,     0,    0}},
    {"oi",    {EN_OW, EN_IH, 0,    0}},
    {"oy",    {EN_OW, EN_IH, 0,    0}},
    {"ai",    {EN_EY, 0,     0,    0}},
    {"ay",    {EN_EY, 0,     0,    0}},
    {"ea",    {EN_IY, 0,     0,    0}},
    {"ee",    {EN_IY, 0,     0,    0}},
    {"ie",    {EN_IY, 0,     0,    0}},
    {"ue",    {EN_UW, 0,     0,    0}},
    {"ui",    {EN_UW, 0,     0,    0}},
    {"ew",    {EN_UW, 0,     0,    0}},
    {"ate",   {EN_EY, EN_T,  0,    0}},
    {"ise",   {EN_S,  EN_AX, 0,    0}},
    {"ize",   {EN_Z,  EN_AX, 0,    0}},
    {"ed",    {EN_D,  0,     0,    0}},
    {"a",     {EN_AE, 0,     0,    0}},
    {"e",     {EN_EH, 0,     0,    0}},
    {"i",     {EN_IH, 0,     0,    0}},
    {"o",     {EN_AO, 0,     0,    0}},
    {"u",     {EN_UH, 0,     0,    0}},
    {"y",     {EN_IH, 0,     0,    0}},
    {"b",     {EN_B,  0,     0,    0}},
    {"c",     {EN_K,  0,     0,    0}},
    {"d",     {EN_D,  0,     0,    0}},
    {"f",     {EN_F,  0,     0,    0}},
    {"g",     {EN_G,  0,     0,    0}},
    {"h",     {EN_HH, 0,     0,    0}},
    {"j",     {EN_JH, 0,     0,    0}},
    {"k",     {EN_K,  0,     0,    0}},
    {"l",     {EN_L,  0,     0,    0}},
    {"m",     {EN_M,  0,     0,    0}},
    {"n",     {EN_N,  0,     0,    0}},
    {"p",     {EN_P,  0,     0,    0}},
    {"q",     {EN_K,  0,     0,    0}},
    {"r",     {EN_R,  0,     0,    0}},
    {"s",     {EN_S,  0,     0,    0}},
    {"t",     {EN_T,  0,     0,    0}},
    {"v",     {EN_V,  0,     0,    0}},
    {"w",     {EN_W,  0,     0,    0}},
    {"x",     {EN_K,  EN_S,  0,    0}},
    {"z",     {EN_Z,  0,     0,    0}},
    {NULL,    {0,     0,     0,    0}}
};

/* convert lowercase ascii word to phoneme tokens */
static int en_grapheme_to_phonemes(const char *word, uint32_t *out, int max_out)
{
    int oi = 0, wlen = (int)strlen(word);
    for (int i = 0; i < wlen && oi < max_out; ) {
        int matched = 0;
        for (int r = 0; g2p_rules[r].grapheme != NULL; r++) {
            const char *g = g2p_rules[r].grapheme;
            int gl = (int)strlen(g);
            if (i + gl > wlen) continue;
            if (strncmp(word + i, g, (size_t)gl) != 0) continue;
            for (int p = 0; p < G2P_MAX_PHONES && g2p_rules[r].phones[p] != 0; p++) {
                if (oi < max_out) out[oi++] = g2p_rules[r].phones[p];
            }
            i += gl; matched = 1; break;
        }
        if (!matched) {
            char c = word[i];
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                if (oi < max_out) out[oi++] = EN_AX;
            }
            i++;
        }
    }
    return oi;
}

/* return pause in seconds for ascii punctuation */
static double en_punctuation_pause(uint32_t cp)
{
    if (cp == 32)              return 0.10;
    if (cp == 44)              return 0.18;
    if (cp == 46)              return 0.35;
    if (cp == 33 || cp == 63)  return 0.36;
    if (cp == 59 || cp == 58)  return 0.24;
    if (cp == 10)              return 0.40;
    if (cp == 45)              return 0.09;
    return 0.0;
}

/* expand digits 0-9 to english words. caller must free memory. */
static char *en_expand_input(const char *in)
{
    static const char *dw[10] = {
        "zero","one","two","three","four",
        "five","six","seven","eight","nine"
    };
    size_t inl = strlen(in), outcap = (inl + 1) * 8 + 16;
    char *out = malloc(outcap);
    if (!out) return NULL;
    out[0] = 0;
    size_t oi = 0;
    for (size_t i = 0; i < inl; ) {
        unsigned char c = (unsigned char)in[i];
        if (c >= '0' && c <= '9') {
            const char *w = dw[c - '0'];
            size_t wl = strlen(w);
            if (oi + wl + 2 >= outcap) { outcap *= 2; out = realloc(out, outcap); }
            memcpy(out + oi, w, wl); oi += wl;
            out[oi++] = ' '; out[oi] = 0;
            i++; continue;
        }
        if (oi + 2 >= outcap) { outcap *= 2; out = realloc(out, outcap); }
        out[oi++] = in[i++]; out[oi] = 0;
    }
    while (oi > 0 && out[oi - 1] == ' ') out[--oi] = 0;
    return out;
}

/* lookup phoneme by codepoint */
static PhonemeDef *en_find_phoneme(uint32_t code)
{
    for (int i = 0; en_phonemes[i].code != 0; i++) {
        if (en_phonemes[i].code == code) return &en_phonemes[i];
    }
    return NULL;
}

/* coarticulation predicates */
static int en_is_vowel_code(uint32_t code) { return (code >= EN_AE && code <= EN_AX); }
static int en_is_nasal_code(uint32_t code) { return (code == EN_M || code == EN_N || code == EN_NG); }
static int en_is_approximant_code(uint32_t code) { return (code == EN_L || code == EN_R || code == EN_W || code == EN_Y); }
static double en_clampd(double x, double lo, double hi) { return x < lo ? lo : (x > hi ? hi : x); }

/* context-sensitive coarticulation to modify phoneme targets */
static void en_coarticulate_context(uint32_t prev_code, uint32_t cur_code, uint32_t next_code, PhonemeDef *out_def)
{
    PhonemeDef *cur = en_find_phoneme(cur_code);
    if (!cur) {
        memset(out_def, 0, sizeof(*out_def));
        out_def->code = cur_code;
        return;
    }
    *out_def = *cur;
    PhonemeDef *prev = (prev_code ? en_find_phoneme(prev_code) : NULL);
    PhonemeDef *next = (next_code ? en_find_phoneme(next_code) : NULL);

    double f1 = (double)cur->f1, f2 = (double)cur->f2, f3 = (double)cur->f3;
    double dur = cur->duration, amp = cur->amp;
    double base_w = 0.20;
    double dur_factor = en_clampd(0.12 / (cur->duration + 0.001), 0.6, 1.6);
    double wPrev = prev ? base_w * dur_factor * 0.9 : 0.0;
    double wNext = next ? base_w * dur_factor * 1.1 : 0.0;

    if (cur->type == vtype_vowel) {
        /* vowel length adjustments for voicing context */
        if (next && next->is_voiced) dur *= 1.15;
        else if (next && !next->is_voiced) dur *= 0.90;

        /* formant blending with neighbors */
        if (prev && prev->type == vtype_vowel) {
            f1 = f1 * (1.0 - wPrev) + prev->f1 * wPrev;
            f2 = f2 * (1.0 - wPrev) + prev->f2 * wPrev;
            f3 = f3 * (1.0 - wPrev) + prev->f3 * wPrev;
        } else if (prev) {
            if (prev_code == EN_P || prev_code == EN_B || prev_code == EN_M) f2 -= 80.0 * wPrev;
            else if (prev_code == EN_T || prev_code == EN_D || prev_code == EN_N || prev_code == EN_L) f2 += 60.0 * wPrev;
            else if (prev_code == EN_K || prev_code == EN_G || prev_code == EN_NG) f2 -= 40.0 * wPrev;
        }

        if (next && next->type == vtype_vowel) {
            f1 = f1 * (1.0 - wNext) + next->f1 * wNext;
            f2 = f2 * (1.0 - wNext) + next->f2 * wNext;
            f3 = f3 * (1.0 - wNext) + next->f3 * wNext;
        } else if (next) {
            if (next_code == EN_P || next_code == EN_B || next_code == EN_M) f2 -= 80.0 * wNext;
            else if (next_code == EN_T || next_code == EN_D || next_code == EN_N || next_code == EN_L) f2 += 60.0 * wNext;
            else if (next_code == EN_K || next_code == EN_G || next_code == EN_NG) f2 -= 40.0 * wNext;
        }

        /* nasal and rhotic coloring */
        if ((next && en_is_nasal_code(next_code)) || (prev && en_is_nasal_code(prev_code))) {
            f2 -= 60.0 * 0.8; f3 -= 140.0 * 0.8; amp *= 0.985;
        }
        if (cur_code == EN_ER || (next && next_code == EN_R) || (prev && prev_code == EN_R)) {
            f3 -= 280.0 * 0.9; f2 -= 40.0 * 0.5;
        }
        f1 = en_clampd(f1, 60.0, 1200.0); f2 = en_clampd(f2, 300.0, 4500.0); f3 = en_clampd(f3, 1000.0, 5000.0);
    } else {
        /* consonant tweaks for voicing and adjacency */
        if (cur->is_voiced) dur *= 1.05; else dur *= 0.95;
        if (next && next->type == vtype_vowel) amp *= 1.06;
        if (prev && prev->type == vtype_vowel) amp *= 0.96;
        if ((cur_code == EN_T || cur_code == EN_D) && next && next->code == EN_Y) f2 += 120.0 * 0.6;
        if (cur->type == vtype_fricative && next && next->type == vtype_vowel && next->f2 > 1800) {
            amp *= 1.05; f1 -= 30.0 * 0.4;
        }
        if (en_is_approximant_code(cur_code) || en_is_nasal_code(cur_code)) {
            double w = (next && next->type == vtype_vowel) ? 0.22 : (prev && prev->type == vtype_vowel) ? 0.18 : 0.0;
            if (w > 0) {
                PhonemeDef *ref = (next && next->type == vtype_vowel) ? next : prev;
                f1 = f1 * (1.0 - w) + ref->f1 * w;
                f2 = f2 * (1.0 - w) + ref->f2 * w;
                f3 = f3 * (1.0 - w) + ref->f3 * w;
            }
        }
        f1 = en_clampd(f1, 60.0, 1200.0); f2 = en_clampd(f2, 200.0, 5000.0); f3 = en_clampd(f3, 600.0, 6000.0);
    }

    out_def->f1 = (int)round(f1);
    out_def->f2 = (int)round(f2);
    out_def->f3 = (int)round(f3);
    out_def->duration = en_clampd(dur, 0.03, 0.45);
    out_def->amp = en_clampd(amp, 0.2, 1.4);
    out_def->code = cur_code;
}
