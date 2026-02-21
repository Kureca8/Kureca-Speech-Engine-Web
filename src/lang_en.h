#pragma once

#include "tts_synth.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

/* Phoneme ID definitions (Unicode private-use area E000–E0FF) */

/* monophthong vowels */
#define EN_AE   0xE000u  /* /æ/  bat        */
#define EN_AA   0xE001u  /* /ɑ/  father/hot */
#define EN_AH   0xE002u  /* /ʌ/  cup        */
#define EN_AO   0xE003u  /* /ɔ/  thought    */
#define EN_EH   0xE005u  /* /ɛ/  bed        */
#define EN_ER   0xE006u  /* /ɝ/  bird       */
#define EN_IH   0xE008u  /* /ɪ/  bit        */
#define EN_IY   0xE009u  /* /i/  fleece     */
#define EN_UH   0xE00Bu  /* /ʊ/  foot       */
#define EN_UW   0xE00Cu  /* /u/  goose      */
#define EN_AX   0xE00Du  /* /ə/  schwa      */

/* diphthong nuclei (2 pseudo-phoneme pairs: onset nucleus) */
#define EN_EY1  0xE010u  /* /eɪ/ onset  face  F1=430 F2=1900 */
#define EN_EY2  0xE011u  /* /eɪ/ glide        F1=270 F2=2290 */
#define EN_AW1  0xE012u  /* /aʊ/ onset  out   F1=750 F2=1200 */
#define EN_AW2  0xE013u  /* /aʊ/ glide        F1=400 F2= 870 */
#define EN_OW1  0xE014u  /* /oʊ/ onset  goat  F1=500 F2= 900 */
#define EN_OW2  0xE015u  /* /oʊ/ glide        F1=300 F2= 700 */
#define EN_OI1  0xE016u  /* /ɔɪ/ onset  boy   F1=570 F2= 840 */
#define EN_OI2  0xE017u  /* /ɔɪ/ glide        F1=270 F2=2290 */

/* Convenience: legacy single-code diphthong aliases map to onset codes */
#define EN_EY   EN_EY1
#define EN_AW   EN_AW1
#define EN_OW   EN_OW1

/* stops */
#define EN_P    0xE020u
#define EN_B    0xE021u
#define EN_T    0xE022u
#define EN_D    0xE023u
#define EN_K    0xE024u
#define EN_G    0xE025u

/* fricatives */
#define EN_F    0xE030u
#define EN_V    0xE031u
#define EN_TH   0xE032u  /* /θ/ thin   */
#define EN_DH   0xE033u  /* /ð/ this   */
#define EN_S    0xE034u
#define EN_Z    0xE035u
#define EN_SH   0xE036u
#define EN_ZH   0xE037u
#define EN_HH   0xE038u

/* affricates */
#define EN_CH   0xE040u
#define EN_JH   0xE041u

/* nasals */
#define EN_M    0xE050u
#define EN_N    0xE051u
#define EN_NG   0xE052u

/* approximants */
#define EN_L    0xE060u
#define EN_R    0xE061u
#define EN_W    0xE062u
#define EN_Y    0xE063u


/* Phoneme acoustic parameter table */

static PhonemeDef en_phonemes[] = {
    /*        code    F1    F2    F3    dur    type           amp    voiced */

    /* /æ/ bat — hyperarticulated: push F1 up, F2 maximally front
     * key contrast partner: /ɑ/ (same F1, much higher F2 here) */
    {EN_AE,   700, 1800, 2550,  0.210, vtype_vowel,     1.12,  1},

    /* /ɑ/ father — very low F2 to contrast /æ/ and /ʌ/ */
    {EN_AA,   750,  980, 2500,  0.230, vtype_vowel,     1.10,  1},

    /* /ʌ/ cup — central: F2 clearly between front and back */
    {EN_AH,   620, 1260, 2500,  0.175, vtype_vowel,     1.06,  1},

    /* /ɔ/ thought — low-back rounded: F1 low, F2 very low */
    {EN_AO,   550,  750, 2500,  0.185, vtype_vowel,     1.05,  1},

    /* /ɛ/ bed — contrast with /æ/: lower F1, similar F2 */
    {EN_EH,   520, 1900, 2600,  0.175, vtype_vowel,     1.06,  1},

    /* /ɝ/ bird — defining cue is F3 depression to ~1600 Hz */
    {EN_ER,   480, 1330, 1600,  0.200, vtype_vowel,     1.02,  1},

    /* /ɪ/ bit — high-front but clearly below /iː/ in F2 */
    {EN_IH,   400, 2050, 2700,  0.165, vtype_vowel,     1.04,  1},

    /* /iː/ fleece — extreme: lowest F1, highest F2 possible */
    {EN_IY,   250, 2400, 3100,  0.210, vtype_vowel,     1.10,  1},

    /* /ʊ/ foot — near-high back: F2 below /ʌ/ but above /uː/ */
    {EN_UH,   420, 1050, 2300,  0.165, vtype_vowel,     1.03,  1},

    /* /uː/ goose — extreme back: lowest F2 in system */
    {EN_UW,   280,  800, 2300,  0.210, vtype_vowel,     1.08,  1},

    /* /ə/ schwa — fast, central, reduced amplitude */
    {EN_AX,   490, 1480, 2400,  0.075, vtype_vowel,     0.82,  1},

    /* =====================================================================
     * DIPHTHONGS — split as onset + nucleus
     * onset durationp slightly longer; nucleus/glide shorter.
     * ===================================================================== */

    /* /eɪ/ face — onset /ɛ/-like, glide drives hard into /iː/ territory */
    {EN_EY1,  480, 1820, 2600,  0.140, vtype_vowel,     1.08,  1},
    {EN_EY2,  250, 2400, 3100,  0.100, vtype_vowel,     0.88,  1},

    /* /aʊ/ out — onset low-open, glide drives deep into /uː/ territory */
    {EN_AW1,  800, 1350, 2600,  0.140, vtype_vowel,     1.07,  1},
    {EN_AW2,  300,  750, 2300,  0.100, vtype_vowel,     0.85,  1},

    /* /oʊ/ goat — onset mid-back, glide into /uː/ */
    {EN_OW1,  530,  820, 2400,  0.130, vtype_vowel,     1.05,  1},
    {EN_OW2,  270,  680, 2250,  0.100, vtype_vowel,     0.86,  1},

    /* /ɔɪ/ boy — onset /ɔ/-low, glide into /iː/ */
    {EN_OI1,  550,  760, 2500,  0.130, vtype_vowel,     1.05,  1},
    {EN_OI2,  260, 2400, 3100,  0.100, vtype_vowel,     0.86,  1},

    /* stops murmur */
    /*        code   F1   F2    F3   dur    type              amp    voiced */

    /* voiced stops: pre-voicing + short duration */
    {EN_B,    200,  700,    0,  0.075, vtype_consonant,  0.82,  1},
    {EN_D,    200, 1800,    0,  0.075, vtype_consonant,  0.80,  1},
    {EN_G,    200, 2800,    0,  0.075, vtype_consonant,  0.78,  1},

    /* voiceless stops: silence + aspiration burst */
    {EN_P,    200,  700,    0,  0.085, vtype_stop,       0.55,  0},
    {EN_T,    200, 1800,    0,  0.090, vtype_stop,       0.57,  0},
    {EN_K,    200, 2800,    0,  0.095, vtype_stop,       0.55,  0},

    {EN_F,   1500, 4000,    0,  0.150, vtype_fricative,  0.38,  0},
    {EN_V,   1500, 4000,    0,  0.140, vtype_fricative,  0.42,  1},
    {EN_TH,   800, 6000,    0,  0.130, vtype_fricative,  0.28,  0},
    {EN_DH,   800, 6000,    0,  0.120, vtype_fricative,  0.32,  1},
    {EN_S,   5500, 8000,    0,  0.170, vtype_fricative,  0.50,  0},  /* strident, bright */
    {EN_Z,   5000, 7500,    0,  0.160, vtype_fricative,  0.52,  1},
    {EN_SH,  2000, 3500,    0,  0.170, vtype_fricative,  0.52,  0},  /* clearly lower than /s/ */
    {EN_ZH,  2000, 3500,    0,  0.160, vtype_fricative,  0.54,  1},
    {EN_HH,   600, 1800,    0,  0.100, vtype_fricative,  0.24,  0},

    /* /tʃ/ /dʒ/ — affricates: lower F2 than /s/, higher than /ʃ/ */
    {EN_CH,  1800, 3200,    0,  0.155, vtype_stop,       0.52,  0},
    {EN_JH,  1800, 3200,    0,  0.145, vtype_stop,       0.52,  1},

    /* NASALS: amplitude boosted — nasal murmur must be audible */
    {EN_M,    270,  900,    0,  0.155, vtype_consonant,  0.72,  1},
    {EN_N,    270, 1100,    0,  0.150, vtype_consonant,  0.72,  1},
    {EN_NG,   270,  800,    0,  0.140, vtype_consonant,  0.68,  1},

    /* APPROXIMANTS: boosted amp — /l r w j/ carry critical transition cues */
    {EN_L,    380,  980, 2550,  0.150, vtype_consonant,  0.80,  1},
    {EN_R,    400, 1060, 1580,  0.140, vtype_consonant,  0.76,  1},  /* F3=1580: strong rhotic */
    {EN_W,    300,  680, 2200,  0.120, vtype_consonant,  0.72,  1},
    {EN_Y,    250, 2200, 3000,  0.110, vtype_consonant,  0.70,  1},

    /* terminator */
    {0, 0, 0, 0, 0.0, 0, 0.0, 0}
};

/*
 * G2P  (Grapheme-to-Phoneme) rules
 *
 * rules are matched longest-first at each position.
 * up to 4 output phonemes per grapheme.
 * diphthongs now properly output two tokens (onset + glide).
 * rules for soft-c, soft-g added.
 * "ough" has multiple pronunciation classes — context resolved in code below.
 *
 * the terminating rule must have grapheme == NULL.
 *   */
#define G2P_MAX_PHONES 5
typedef struct {
    const char *grapheme;
    uint32_t    phones[G2P_MAX_PHONES];
    /* extra flags (reserved, set 0) */
} G2PRule;

static G2PRule g2p_rules[] = {

    /* 4-letter patterns */
    {"tion",  {EN_SH, EN_AX, EN_N,  0,    0}},
    {"sion",  {EN_ZH, EN_AX, EN_N,  0,    0}},
    {"tial",  {EN_SH, EN_AX, EN_L,  0,    0}},
    {"cial",  {EN_SH, EN_AX, EN_L,  0,    0}},
    {"tious", {EN_SH, EN_AX, EN_S,  0,    0}},
    {"cious", {EN_SH, EN_AX, EN_S,  0,    0}},
    {"eous",  {EN_IY, EN_AX, EN_S,  0,    0}},
    {"ight",  {EN_EY1,EN_EY2,EN_T,  0,    0}},  /* night, right */
    {"ough",  {EN_AW1,EN_AW2,0,     0,    0}},  /* rough ≈ /ʌf/ — see special handler below */
    {"ould",  {EN_UH, EN_D,  0,     0,    0}},  /* could, would, should */
    {"ough",  {EN_AO, 0,     0,     0,    0}},  /* thought  — fallback (code picks first match) */

    /* 3-letter patterns */
    {"tch",   {EN_CH, 0,     0,     0,    0}},
    {"dge",   {EN_JH, 0,     0,     0,    0}},
    {"igh",   {EN_EY1,EN_EY2,0,     0,    0}},
    {"ght",   {EN_T,  0,     0,     0,    0}},
    {"eau",   {EN_OW1,EN_OW2,0,     0,    0}},
    {"ear",   {EN_IH, EN_R,  0,     0,    0}},
    {"ire",   {EN_EY1,EN_EY2,EN_R,  0,    0}},  /* fire: /faɪr/ */
    {"ion",   {EN_EY1,EN_AX, EN_N,  0,    0}},
    {"ure",   {EN_UH, EN_R,  0,     0,    0}},
    {"ore",   {EN_AO, EN_R,  0,     0,    0}},
    {"are",   {EN_EH, EN_R,  0,     0,    0}},
    {"air",   {EN_EH, EN_R,  0,     0,    0}},
    {"our",   {EN_AW1,EN_AW2,EN_R,  0,    0}},
    {"oul",   {EN_UH, EN_L,  0,     0,    0}},  /* soul special-cased by 'ou' below */
    {"wor",   {EN_ER, EN_R,  0,     0,    0}},  /* word, work */
    {"war",   {EN_AO, EN_R,  0,     0,    0}},  /* warm, ward */

    /* 2-letter digraphs - consonants */
    {"ch",    {EN_CH, 0,     0,     0,    0}},
    {"sh",    {EN_SH, 0,     0,     0,    0}},
    {"th",    {EN_TH, 0,     0,     0,    0}},  /* default unvoiced; voiced handled by coart. */
    {"ph",    {EN_F,  0,     0,     0,    0}},
    {"wh",    {EN_W,  0,     0,     0,    0}},
    {"ck",    {EN_K,  0,     0,     0,    0}},
    {"ng",    {EN_NG, 0,     0,     0,    0}},
    {"nk",    {EN_NG, EN_K,  0,     0,    0}},
    {"gn",    {EN_N,  0,     0,     0,    0}},
    {"kn",    {EN_N,  0,     0,     0,    0}},
    {"wr",    {EN_R,  0,     0,     0,    0}},
    {"dg",    {EN_JH, 0,     0,     0,    0}},
    {"gh",    {0,     0,     0,     0,    0}},  /* typically silent */
    {"qu",    {EN_K,  EN_W,  0,     0,    0}},
    {"sc",    {EN_S,  0,     0,     0,    0}},

    /* 2-letter digraphs - vowels */
    {"aw",    {EN_AO, 0,     0,     0,    0}},
    {"ow",    {EN_OW1,EN_OW2,0,     0,    0}},
    {"oo",    {EN_UW, 0,     0,     0,    0}},
    {"ou",    {EN_AW1,EN_AW2,0,     0,    0}},
    {"oi",    {EN_OI1,EN_OI2,0,     0,    0}},
    {"oy",    {EN_OI1,EN_OI2,0,     0,    0}},
    {"ai",    {EN_EY1,EN_EY2,0,     0,    0}},
    {"ay",    {EN_EY1,EN_EY2,0,     0,    0}},
    {"ea",    {EN_IY, 0,     0,     0,    0}},
    {"ee",    {EN_IY, 0,     0,     0,    0}},
    {"ie",    {EN_IY, 0,     0,     0,    0}},
    {"ue",    {EN_UW, 0,     0,     0,    0}},
    {"ui",    {EN_UW, 0,     0,     0,    0}},
    {"ew",    {EN_UW, 0,     0,     0,    0}},

    /* common word-ending patterns */
    {"ate",   {EN_EY1,EN_EY2,EN_T,  0,    0}},
    {"ite",   {EN_EY1,EN_EY2,EN_T,  0,    0}},
    {"ise",   {EN_EY1,EN_EY2,EN_Z,  0,    0}},
    {"ize",   {EN_EY1,EN_EY2,EN_Z,  0,    0}},
    {"ed",    {EN_D,  0,     0,     0,    0}},  /* special case handled in code */

    /* single vowel letters (default short value) */
    {"a",     {EN_AE, 0,     0,     0,    0}},
    {"e",     {EN_EH, 0,     0,     0,    0}},
    {"i",     {EN_IH, 0,     0,     0,    0}},
    {"o",     {EN_AO, 0,     0,     0,    0}},
    {"u",     {EN_AH, 0,     0,     0,    0}},  /* cup: /ʌ/ is default for 'u' */
    {"y",     {EN_IY, 0,     0,     0,    0}},

    /* single consonant letters */
    {"b",     {EN_B,  0,     0,     0,    0}},
    {"c",     {EN_K,  0,     0,     0,    0}},  /* hard-c default; soft-c (before e/i) below */
    {"d",     {EN_D,  0,     0,     0,    0}},
    {"f",     {EN_F,  0,     0,     0,    0}},
    {"g",     {EN_G,  0,     0,     0,    0}},  /* hard-g default */
    {"h",     {EN_HH, 0,     0,     0,    0}},
    {"j",     {EN_JH, 0,     0,     0,    0}},
    {"k",     {EN_K,  0,     0,     0,    0}},
    {"l",     {EN_L,  0,     0,     0,    0}},
    {"m",     {EN_M,  0,     0,     0,    0}},
    {"n",     {EN_N,  0,     0,     0,    0}},
    {"p",     {EN_P,  0,     0,     0,    0}},
    {"q",     {EN_K,  0,     0,     0,    0}},
    {"r",     {EN_R,  0,     0,     0,    0}},
    {"s",     {EN_S,  0,     0,     0,    0}},
    {"t",     {EN_T,  0,     0,     0,    0}},
    {"v",     {EN_V,  0,     0,     0,    0}},
    {"w",     {EN_W,  0,     0,     0,    0}},
    {"x",     {EN_K,  EN_S,  0,     0,    0}},
    {"z",     {EN_Z,  0,     0,     0,    0}},

    {NULL,    {0,     0,     0,     0,    0}}
};

/*
 * internal helper: match a G2P rule at position pos in lowercased word.
 * returns 1 if match, 0 otherwise.
 *   */
static int g2p_match_at(const char *lw, int wlen, int pos, const G2PRule *rule)
{
    int gl = (int)strlen(rule->grapheme);
    if (pos + gl > wlen) return 0;
    for (int k = 0; k < gl; k++) {
        if (lw[pos + k] != rule->grapheme[k]) return 0;
    }
    return 1;
}

/*
 * Soft-C / Soft-G detection
 * c /s/ before e, i, y
 * g /dʒ/ before e, i, y  (with exceptions — not handled here)
 *   */
static int is_soft_vowel(char c)
{
    return (c == 'e' || c == 'i' || c == 'y');
}

/*
 * en_grapheme_to_phonemes
 *
 * converts a single whitespace-free word to a phoneme token array.
 * returns the number of phoneme tokens written.
 * caller supplies out[max_out].
 *   */
static int en_grapheme_to_phonemes(const char *word, uint32_t *out, int max_out)
{
    int wlen = (int)strlen(word);
    if (!wlen || !out || max_out <= 0) return 0;

    char *lw = (char*)malloc((size_t)wlen + 1);
    if (!lw) return 0;
    for (int i = 0; i < wlen; i++) lw[i] = (char)tolower((unsigned char)word[i]);
    lw[wlen] = '\0';

    int oi = 0;

    for (int i = 0; i < wlen && oi < max_out; ) {

        /* special: silent trailing 'e' (magic-e rule) */
        if (lw[i] == 'e' && i == wlen - 1 && wlen > 2) {
            i++; continue;
        }

        /* special: soft-c before e/i/y /s/ */
        if (lw[i] == 'c' && i + 1 < wlen && is_soft_vowel(lw[i + 1])) {
            if (oi < max_out) out[oi++] = EN_S;
            i++; continue;
        }

        /* special: soft-g before e/i/y /dʒ/ */
        if (lw[i] == 'g' && i + 1 < wlen && is_soft_vowel(lw[i + 1])) {
            if (oi < max_out) out[oi++] = EN_JH;
            i++; continue;
        }

        /* special: voiced /ð/ for "th" between vowels or word-initial "the" */
        if (lw[i] == 't' && i + 1 < wlen && lw[i + 1] == 'h') {
            uint32_t ph = EN_TH;  /* default voiceless */
            /* voiced if between two vowels */
            if (i > 0 && strchr("aeiou", lw[i - 1])) {
                if (i + 2 < wlen && strchr("aeiou", lw[i + 2])) ph = EN_DH;
            }
            /* common function words: this, the, that, these, those, there, their, them, they, though, than, then */
            /* if word starts with "the"/"thi"/"tho"/"tha"/"the" and len<=5 voiced */
            if (i == 0 && wlen <= 6 && (lw[1] == 'h') &&
                (lw[2] == 'e' || lw[2] == 'i' || lw[2] == 'o' || lw[2] == 'a' || lw[2] == '\0')) {
                ph = EN_DH;
                }
                if (oi < max_out) out[oi++] = ph;
                i += 2; continue;
        }

        /* special: "ough" — classify by following/preceding context */
        if (i + 3 < wlen && lw[i]=='o' && lw[i+1]=='u' && lw[i+2]=='g' && lw[i+3]=='h') {
            /* "ough" + t /ɔ/ (thought, bought) */
            if (i + 4 < wlen && lw[i + 4] == 't') {
                if (oi < max_out) out[oi++] = EN_AO;
            }
            /* "rough","tough","enough" /ʌ/ */
            else if (i + 4 >= wlen || lw[i + 4] == ' ') {
                if (oi < max_out) out[oi++] = EN_AH;
            }
            /* "through"  /u/ */
            else if (i > 0 && lw[i - 1] == 'r') {
                if (oi < max_out) out[oi++] = EN_UW;
            }
            /* default: /oʊ/ (dough) */
            else {
                if (oi < max_out) out[oi++] = EN_OW1;
                if (oi < max_out) out[oi++] = EN_OW2;
            }
            i += 4; continue;
        }

        /* special: "ed" suffix voiced /d/, or silent after t/d */
        if (i > 0 && lw[i] == 'e' && i + 1 == wlen - 1 && lw[i + 1] == 'd') {
            char prev = lw[i - 1];
            if (prev == 't' || prev == 'd') {
                /* pronounced /ɪd/ */
                if (oi < max_out) out[oi++] = EN_IH;
                if (oi < max_out) out[oi++] = EN_D;
            } else {
                /* just /d/ */
                if (oi < max_out) out[oi++] = EN_D;
            }
            i += 2; continue;
        }

        /* general: find longest matching rule */
        int best_r   = -1;
        int best_len =  0;
        for (int r = 0; g2p_rules[r].grapheme != NULL; r++) {
            int gl = (int)strlen(g2p_rules[r].grapheme);
            if (gl <= best_len) continue;  /* can't beat what we have */
                if (lw[i] != g2p_rules[r].grapheme[0]) continue;
                if (g2p_match_at(lw, wlen, i, &g2p_rules[r])) {
                    best_len = gl;
                    best_r   = r;
                }
        }

        if (best_r >= 0) {
            for (int p = 0; p < G2P_MAX_PHONES; p++) {
                uint32_t ph = g2p_rules[best_r].phones[p];
                if (!ph) break;
                if (oi < max_out) out[oi++] = ph;
            }
            i += best_len;
            continue;
        }

        /* fallback: explicit single-char switch (safety net) */
        switch (lw[i]) {
            case 'a': if (oi<max_out) out[oi++]=EN_AE;  break;
            case 'b': if (oi<max_out) out[oi++]=EN_B;   break;
            case 'c': if (oi<max_out) out[oi++]=EN_K;   break;
            case 'd': if (oi<max_out) out[oi++]=EN_D;   break;
            case 'e': if (oi<max_out) out[oi++]=EN_EH;  break;
            case 'f': if (oi<max_out) out[oi++]=EN_F;   break;
            case 'g': if (oi<max_out) out[oi++]=EN_G;   break;
            case 'h': if (oi<max_out) out[oi++]=EN_HH;  break;
            case 'i': if (oi<max_out) out[oi++]=EN_IH;  break;
            case 'j': if (oi<max_out) out[oi++]=EN_JH;  break;
            case 'k': if (oi<max_out) out[oi++]=EN_K;   break;
            case 'l': if (oi<max_out) out[oi++]=EN_L;   break;
            case 'm': if (oi<max_out) out[oi++]=EN_M;   break;
            case 'n': if (oi<max_out) out[oi++]=EN_N;   break;
            case 'o': if (oi<max_out) out[oi++]=EN_AO;  break;
            case 'p': if (oi<max_out) out[oi++]=EN_P;   break;
            case 'q': if (oi<max_out) out[oi++]=EN_K;   break;
            case 'r': if (oi<max_out) out[oi++]=EN_R;   break;
            case 's': if (oi<max_out) out[oi++]=EN_S;   break;
            case 't': if (oi<max_out) out[oi++]=EN_T;   break;
            case 'u': if (oi<max_out) out[oi++]=EN_AH;  break;
            case 'v': if (oi<max_out) out[oi++]=EN_V;   break;
            case 'w': if (oi<max_out) out[oi++]=EN_W;   break;
            case 'x': if (oi<max_out) out[oi++]=EN_K;
            if (oi<max_out) out[oi++]=EN_S;   break;
            case 'y': if (oi<max_out) out[oi++]=EN_Y;   break;
            case 'z': if (oi<max_out) out[oi++]=EN_Z;   break;
            default:  if (oi<max_out) out[oi++]=EN_AX;  break;  /* unknown schwa */
        }
        i++;
    }

    free(lw);
    return oi;
}

/*
 * punctuation pause table
 * returns recommended pause in seconds for a given ASCII codepoint.
 *   */
static double en_punctuation_pause(uint32_t cp)
{
    switch (cp) {
        case ' ':  return 0.08;   /* inter-word space */
        case ',':  return 0.18;   /* comma */
        case ';':  return 0.24;   /* semicolon */
        case ':':  return 0.22;   /* colon */
        case '.':  return 0.38;   /* full stop */
        case '!':  return 0.40;   /* exclamation */
        case '?':  return 0.40;   /* question */
        case '-':  return 0.10;   /* hyphen (in-word) */
        case 0x2013: return 0.18; /* en-dash – */
        case 0x2014: return 0.28; /* em-dash — */
        case '\n': return 0.45;   /* newline / sentence break */
        case '\r': return 0.00;   /* ignore CR */
        case '(':  return 0.06;
        case ')':  return 0.10;
        case '"':  return 0.04;
        case '\'': return 0.00;   /* apostrophe: no pause */
        default:   return 0.00;
    }
}

/*
 * Number-to-words expansion
 *
 * Handles: 0–19 (special), 20–99, 100–999, 1000–99999.
 * Caller must free() the returned string.
 *   */
static const char *en_ones[] = {
    "zero","one","two","three","four","five","six","seven","eight","nine",
    "ten","eleven","twelve","thirteen","fourteen","fifteen","sixteen",
    "seventeen","eighteen","nineteen"
};
static const char *en_tens[] = {
    "","","twenty","thirty","forty","fifty","sixty","seventy","eighty","ninety"
};

static void en_append(char **buf, size_t *cap, size_t *len, const char *s)
{
    size_t sl = strlen(s);
    while (*len + sl + 2 > *cap) { *cap *= 2; *buf = (char*)realloc(*buf, *cap); }
    memcpy(*buf + *len, s, sl);
    *len += sl;
    (*buf)[*len] = '\0';
}

static void en_num_to_words(int n, char **buf, size_t *cap, size_t *len)
{
    if (n < 0) { en_append(buf, cap, len, "minus "); n = -n; }
    if (n < 20) { en_append(buf, cap, len, en_ones[n]); en_append(buf, cap, len, " "); return; }
    if (n < 100) {
        en_append(buf, cap, len, en_tens[n / 10]);
        if (n % 10) { en_append(buf, cap, len, "-"); en_append(buf, cap, len, en_ones[n % 10]); }
        en_append(buf, cap, len, " "); return;
    }
    if (n < 1000) {
        en_num_to_words(n / 100, buf, cap, len);
        en_append(buf, cap, len, "hundred ");
        if (n % 100) en_num_to_words(n % 100, buf, cap, len);
        return;
    }
    if (n < 1000000) {
        en_num_to_words(n / 1000, buf, cap, len);
        en_append(buf, cap, len, "thousand ");
        if (n % 1000) en_num_to_words(n % 1000, buf, cap, len);
        return;
    }
    /* fallback: digit-by-digit */
    char tmp[32]; snprintf(tmp, sizeof(tmp), "%d", n);
    for (int k = 0; tmp[k]; k++) {
        en_append(buf, cap, len, en_ones[tmp[k] - '0']);
        en_append(buf, cap, len, " ");
    }
}

/*
 * en_expand_input
 *
 * converts digits in the input string to English words.
 * also normalises multiple spaces to single space.
 * caller must free() the returned string.
 *   */
static char *en_expand_input(const char *in)
{
    size_t cap = strlen(in) * 10 + 64;
    char *out = (char*)malloc(cap);
    if (!out) return NULL;
    size_t oi = 0;
    out[0] = '\0';

    for (size_t i = 0; in[i]; ) {
        if ((unsigned char)in[i] >= '0' && (unsigned char)in[i] <= '9') {
            /* parse full integer */
            int num = 0;
            while (in[i] >= '0' && in[i] <= '9') { num = num * 10 + (in[i] - '0'); i++; }
            en_num_to_words(num, &out, &cap, &oi);
        } else {
            if (oi + 4 >= cap) { cap *= 2; out = (char*)realloc(out, cap); }
            /* collapse multiple whitespace */
            if (in[i] == ' ' || in[i] == '\t') {
                if (oi > 0 && out[oi - 1] != ' ') out[oi++] = ' ';
                i++;
            } else {
                out[oi++] = in[i++];
            }
            out[oi] = '\0';
        }
    }
    /* strip trailing space */
    while (oi > 0 && out[oi - 1] == ' ') out[--oi] = '\0';
    return out;
}

/*
 * Phoneme lookup
 *   */
static PhonemeDef *en_find_phoneme(uint32_t code)
{
    for (int i = 0; en_phonemes[i].code != 0; i++) {
        if (en_phonemes[i].code == code) return &en_phonemes[i];
    }
    return NULL;
}

/* Phoneme classification helpers */
static inline int en_is_vowel(uint32_t c)
{
    return (c >= EN_AE && c <= EN_AX) ||
    (c >= EN_EY1 && c <= EN_OI2);
}
static inline int en_is_nasal(uint32_t c) { return c == EN_M || c == EN_N || c == EN_NG; }
static inline int en_is_approximant(uint32_t c) { return c == EN_L || c == EN_R || c == EN_W || c == EN_Y; }
static inline int en_is_stop(uint32_t c) { return c >= EN_P && c <= EN_G; }

static inline double en_clamp(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

/*
 * en_coarticulate_context
 *
 * produces a context-modified copy of the phoneme definition.
 * model:
 *  vowels:
 *   - duration: voiced codas lengthen (+10%), voiceless shorten (-8%).
 *   - F2 locus transitions from adjacent stops (Delattre model).
 *   - nasal anticipatory lowering of F1/F2, reduction of F3.
 *   - rhotic context: lower F3 by up to ~300 Hz.
 *  consonants:
 *   - stops: F2 blends toward adjacent vowel target (locus equation).
 *   - approximants/nasals: formants attract adjacent vowel.
 *   - fricatives: slight amplitude boost before high-F2 vowels.
 *   - /th/ voiced between vowels target DH parameters. */
static void en_coarticulate_context(
    uint32_t prev_code, uint32_t cur_code, uint32_t next_code,
    PhonemeDef *out_def)
{
    PhonemeDef *cur = en_find_phoneme(cur_code);
    if (!cur) {
        if (out_def) { memset(out_def, 0, sizeof(*out_def)); out_def->code = cur_code; }
        return;
    }
    *out_def = *cur;

    PhonemeDef *prev = prev_code ? en_find_phoneme(prev_code) : NULL;
    PhonemeDef *next = next_code ? en_find_phoneme(next_code) : NULL;

    double f1  = (double)cur->f1;
    double f2  = (double)cur->f2;
    double f3  = (double)cur->f3;
    double dur = cur->duration;
    double amp = cur->amp;

    if (en_is_vowel(cur_code)) {
        /* duration: open vs. closed syllable voicing (stronger contrast) */
        if (next && next->is_voiced == 0) dur *= 0.88;
        else if (next && next->is_voiced == 1) dur *= 1.14;

        /* F2 locus transitions from stops (stronger blending = clearer place cues) */
        const double LOCUS_W = 0.30;  /* was 0.22 — stronger locus bleeding */
        if (prev && en_is_stop(prev_code)) {
            f2 = f2 * (1.0 - LOCUS_W) + (double)prev->f2 * LOCUS_W;
        }
        if (next && en_is_stop(next_code)) {
            f2 = f2 * (1.0 - LOCUS_W * 0.85) + (double)next->f2 * (LOCUS_W * 0.85);
        }

        /* nasal context: stronger F1/F2 lowering for clear nasalization */
        if ((prev && en_is_nasal(prev_code)) || (next && en_is_nasal(next_code))) {
            f1 -= 55.0;
            f2 -= 90.0;
            f3 -= 200.0;
            amp *= 0.94;
        }

        /* rhotic context: F3 lowered */
        if (cur_code == EN_ER ||
            (next && next_code == EN_R) ||
            (prev && prev_code == EN_R)) {
            double rhotic_pull = (cur_code == EN_ER) ? 1.0 : 0.55;
        f3 -= 300.0 * rhotic_pull;
        f2 -= 40.0  * rhotic_pull;
            }

            /* /l/ dark-l: F1 raise, F2 pull-down pre/post */
            if ((prev && prev_code == EN_L) || (next && next_code == EN_L)) {
                double lw = 0.15;
                f2 = f2 * (1.0 - lw) + 900.0 * lw;
            }

            /* lip rounding from /w/: lowers F1 and F2 */
            if (prev && prev_code == EN_W) { f1 -= 30.0; f2 -= 80.0; }
            if (next && next_code == EN_W) { f2 -= 60.0; }

            /* palatalization from /j/: raises F2 */
            if (prev && prev_code == EN_Y) { f2 += 120.0; }

            /* clamp to acoustic limits */
            f1 = en_clamp(f1,  60.0, 1200.0);
            f2 = en_clamp(f2, 300.0, 4500.0);
            f3 = en_clamp(f3, 900.0, 5000.0);
            dur = en_clamp(dur, 0.04, 0.50);
            amp = en_clamp(amp, 0.20, 1.60);

    } else {
        /* CONSONANT context */

        /* voiced consonants slightly longer; voiceless slightly shorter */
        if (cur->is_voiced) dur *= 1.04;
        else                dur *= 0.96;

        /* amplitude: louder before/after vowels — consonants get swallowed otherwise */
        if (next && en_is_vowel(next_code))  amp *= 1.10;
        if (prev && en_is_vowel(prev_code))  amp *= 0.98;

        /* stops: F2 transition toward adjacent vowel (locus blending) */
        if (en_is_stop(cur_code)) {
            double sblend = 0.35;  /* was 0.30 */
            PhonemeDef *ref = (next && en_is_vowel(next_code)) ? next
            : (prev && en_is_vowel(prev_code)) ? prev : NULL;
            if (ref) {
                f1 = f1 * (1.0 - sblend * 0.5) + (double)ref->f1 * (sblend * 0.5);
                f2 = f2 * (1.0 - sblend)        + (double)ref->f2 * sblend;
            }
        }

        /* approximants / nasals: formants attract adjacent vowel (stronger) */
        if (en_is_approximant(cur_code) || en_is_nasal(cur_code)) {
            double w = 0.0;
            PhonemeDef *ref = NULL;
            if (next && en_is_vowel(next_code)) { w = 0.28; ref = next; }  /* was 0.22 */
                else if (prev && en_is_vowel(prev_code)) { w = 0.22; ref = prev; }  /* was 0.18 */
                    if (ref && w > 0.0) {
                        f1 = f1 * (1.0 - w) + (double)ref->f1 * w;
                        f2 = f2 * (1.0 - w) + (double)ref->f2 * w;
                        if (ref->f3 > 0) f3 = f3 * (1.0 - w) + (double)ref->f3 * w;
                    }
        }

        /* palatalization: /t/ or /d/ before /j/ (→ affricate-like shift) */
        if ((cur_code == EN_T || cur_code == EN_D) && next && next_code == EN_Y) {
            f2 += 250.0;
        }

        /* fricative boost before/after vowels — especially important for /f θ/ */
        if (cur->type == vtype_fricative) {
            if (next && en_is_vowel(next_code)) amp *= 1.08;
            if (prev && en_is_vowel(prev_code)) amp *= 1.05;
        }

        /* /r/ in onset position: enforce low F3 */
        if (cur_code == EN_R && next && en_is_vowel(next_code)) {
            f3 = en_clamp(f3, 900.0, 1650.0);
        }

        f1 = en_clamp(f1,  60.0, 1200.0);
        f2 = en_clamp(f2, 200.0, 7000.0);
        f3 = en_clamp(f3, 600.0, 6500.0);
        dur = en_clamp(dur, 0.02, 0.40);
        amp = en_clamp(amp, 0.15, 1.60);
    }

    out_def->f1       = (int)round(f1);
    out_def->f2       = (int)round(f2);
    out_def->f3       = (f3 > 10.0) ? (int)round(f3) : 0;
    out_def->duration = dur;
    out_def->amp      = amp;
    out_def->code     = cur_code;
}

/*
 * en_phoneme_name  — debug helper
 * returns a human-readable IPA-like label for a phoneme code. */
static const char *en_phoneme_name(uint32_t code)
{
    switch (code) {
        case EN_AE:  return "/æ/ bat";
        case EN_AA:  return "/ɑ/ father";
        case EN_AH:  return "/ʌ/ cup";
        case EN_AO:  return "/ɔ/ thought";
        case EN_EH:  return "/ɛ/ bed";
        case EN_ER:  return "/ɝ/ bird";
        case EN_IH:  return "/ɪ/ bit";
        case EN_IY:  return "/i/ fleece";
        case EN_UH:  return "/ʊ/ foot";
        case EN_UW:  return "/u/ goose";
        case EN_AX:  return "/ə/ schwa";
        case EN_EY1: return "/eɪ/ onset";
        case EN_EY2: return "/eɪ/ glide";
        case EN_AW1: return "/aʊ/ onset";
        case EN_AW2: return "/aʊ/ glide";
        case EN_OW1: return "/oʊ/ onset";
        case EN_OW2: return "/oʊ/ glide";
        case EN_OI1: return "/ɔɪ/ onset";
        case EN_OI2: return "/ɔɪ/ glide";
        case EN_P:   return "/p/";
        case EN_B:   return "/b/";
        case EN_T:   return "/t/";
        case EN_D:   return "/d/";
        case EN_K:   return "/k/";
        case EN_G:   return "/g/";
        case EN_F:   return "/f/";
        case EN_V:   return "/v/";
        case EN_TH:  return "/θ/ thin";
        case EN_DH:  return "/ð/ this";
        case EN_S:   return "/s/";
        case EN_Z:   return "/z/";
        case EN_SH:  return "/ʃ/";
        case EN_ZH:  return "/ʒ/";
        case EN_HH:  return "/h/";
        case EN_CH:  return "/tʃ/";
        case EN_JH:  return "/dʒ/";
        case EN_M:   return "/m/";
        case EN_N:   return "/n/";
        case EN_NG:  return "/ŋ/";
        case EN_L:   return "/l/";
        case EN_R:   return "/r/";
        case EN_W:   return "/w/";
        case EN_Y:   return "/j/";
        default:     return "<?>";
    }
}
