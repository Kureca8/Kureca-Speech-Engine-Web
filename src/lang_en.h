#pragma once

#include "tts_synth.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>

#define EN_AE   0xE000u  // ae trap
#define EN_AA   0xE001u  // aa lot
#define EN_AH   0xE002u  // ah strut
#define EN_AO   0xE003u  // ao thought
#define EN_EH   0xE005u  // eh dress
#define EN_ER   0xE006u  // er nurse
#define EN_IH   0xE008u  // ih kit
#define EN_IY   0xE009u  // iy fleece
#define EN_UH   0xE00Bu  // uh foot
#define EN_UW   0xE00Cu  // uw goose
#define EN_AX   0xE00Du  // ax schwa

// diphthongs split into onset and glide for smooth formant trajectory
#define EN_EY1  0xE010u  // ey onset
#define EN_EY2  0xE011u  // ey glide
#define EN_AW1  0xE012u  // aw onset
#define EN_AW2  0xE013u  // aw glide
#define EN_OW1  0xE014u  // ow onset
#define EN_OW2  0xE015u  // ow glide
#define EN_OI1  0xE016u  // oi onset
#define EN_OI2  0xE017u  // oi glide
#define EN_AY1  0xE018u  // ay onset
#define EN_AY2  0xE019u  // ay glide

// convenience aliases
#define EN_EY   EN_EY1
#define EN_AW   EN_AW1
#define EN_OW   EN_OW1
#define EN_AY   EN_AY1

// stops
#define EN_P    0xE020u
#define EN_B    0xE021u
#define EN_T    0xE022u
#define EN_D    0xE023u
#define EN_K    0xE024u
#define EN_G    0xE025u

// fricatives
#define EN_F    0xE030u
#define EN_V    0xE031u
#define EN_TH   0xE032u  // th thin
#define EN_DH   0xE033u  // dh this
#define EN_S    0xE034u
#define EN_Z    0xE035u
#define EN_SH   0xE036u
#define EN_ZH   0xE037u
#define EN_HH   0xE038u

// affricates
#define EN_CH   0xE040u
#define EN_JH   0xE041u

// nasals
#define EN_M    0xE050u
#define EN_N    0xE051u
#define EN_NG   0xE052u

// approximants and liquids
#define EN_L    0xE060u
#define EN_R    0xE061u
#define EN_W    0xE062u
#define EN_Y    0xE063u

// syllabic consonants unstressed syllable nuclei
#define EN_EL   0xE064u  // el syllabic l
#define EN_EN   0xE065u  // en syllabic n
#define EN_EM   0xE066u  // em syllabic m


// formant table
// male targets from hillenbrand and peterson and barney
// durations scaled to 120 wpm baseline
// amp relative acoustic prominence vowels near 1.0 fricatives lower

static PhonemeDef en_phonemes[] = {
    // code      F1    F2    F3    dur    type              amp   voiced

    // tense vowels
    {EN_IY,   270, 2290, 3010,  0.200, vtype_vowel,      1.00,  1},  // fleece
    {EN_UW,   310,  870, 2240,  0.180, vtype_vowel,      0.96,  1},  // goose
    {EN_EY1,  530, 1840, 2480,  0.120, vtype_vowel,      1.00,  1},  // face onset
    {EN_EY2,  270, 2290, 3010,  0.110, vtype_vowel,      0.95,  1},  // face glide
    {EN_OW1,  570,  840, 2410,  0.120, vtype_vowel,      0.98,  1},  // goat onset
    {EN_OW2,  310,  870, 2240,  0.100, vtype_vowel,      0.92,  1},  // goat glide

    // lax vowels
    {EN_IH,   400, 1920, 2550,  0.155, vtype_vowel,      0.97,  1},  // kit
    {EN_EH,   580, 1800, 2610,  0.155, vtype_vowel,      0.98,  1},  // dress
    {EN_AE,   800, 1720, 2410,  0.190, vtype_vowel,      1.02,  1},  // trap
    {EN_AH,   640, 1190, 2390,  0.160, vtype_vowel,      0.99,  1},  // strut
    {EN_AO,   560,  840, 2410,  0.190, vtype_vowel,      0.99,  1},  // thought
    {EN_AA,   730,  980, 2580,  0.195, vtype_vowel,      1.00,  1},  // lot
    {EN_UH,   490, 1020, 2240,  0.150, vtype_vowel,      0.95,  1},  // foot
    {EN_ER,   490, 1350, 1690,  0.200, vtype_vowel,      0.97,  1},  // nurse

    // schwa reduced shorter
    {EN_AX,   500, 1500, 2500,  0.080, vtype_vowel,      0.82,  1},

    // diphthongs price mouth choice
    {EN_AY1,  730,  980, 2580,  0.130, vtype_vowel,      1.00,  1},  // price onset
    {EN_AY2,  270, 2290, 3010,  0.110, vtype_vowel,      0.88,  1},  // price glide
    {EN_AW1,  800, 1200, 2400,  0.130, vtype_vowel,      1.00,  1},  // mouth onset
    {EN_AW2,  310,  870, 2240,  0.110, vtype_vowel,      0.88,  1},  // mouth glide
    {EN_OI1,  560,  840, 2410,  0.130, vtype_vowel,      0.99,  1},  // choice onset
    {EN_OI2,  270, 2290, 3010,  0.110, vtype_vowel,      0.88,  1},  // choice glide

    // syllabic consonants
    {EN_EL,   380, 1100, 2400,  0.120, vtype_consonant,  0.80,  1},  // syllabic l
    {EN_EN,   280, 1200, 2500,  0.110, vtype_consonant,  0.78,  1},  // syllabic n
    {EN_EM,   280,  900, 2200,  0.110, vtype_consonant,  0.78,  1},  // syllabic m

    // stops burst params handled in expand_phone f2 used as locus
    {EN_P,    200,  800,    0,  0.080, vtype_stop,       0.75,  0},
    {EN_B,    200,  800,    0,  0.080, vtype_stop,       0.78,  1},
    {EN_T,    200, 1800,    0,  0.075, vtype_stop,       0.78,  0},
    {EN_D,    200, 1800,    0,  0.075, vtype_stop,       0.80,  1},
    {EN_K,    200, 2500,    0,  0.085, vtype_stop,       0.76,  0},
    {EN_G,    200, 2500,    0,  0.085, vtype_stop,       0.78,  1},

    // fricatives bands chosen to avoid nyquist issues
    {EN_F,   3000, 6500,    0,  0.120, vtype_fricative,  0.40,  0},
    {EN_V,   2500, 6000,    0,  0.110, vtype_fricative,  0.44,  1},
    {EN_TH,  1800, 6000,    0,  0.130, vtype_fricative,  0.32,  0},
    {EN_DH,  1600, 5500,    0,  0.120, vtype_fricative,  0.36,  1},
    {EN_S,   3500, 7000,    0,  0.140, vtype_fricative,  0.50,  0},
    {EN_Z,   3000, 6500,    0,  0.130, vtype_fricative,  0.52,  1},
    {EN_SH,  1800, 4500,    0,  0.145, vtype_fricative,  0.54,  0},
    {EN_ZH,  1800, 4500,    0,  0.130, vtype_fricative,  0.56,  1},
    {EN_HH,   400, 2000,    0,  0.090, vtype_fricative,  0.34,  0},

    // affricates stop closure plus fricative release handled in expand_phone
    {EN_CH,  1800, 3500,    0,  0.170, vtype_stop,       0.78,  0},
    {EN_JH,  1800, 3500,    0,  0.160, vtype_stop,       0.80,  1},

    // nasals antiformant around 800-1200 hertz
    {EN_M,    280,  900, 2200,  0.090, vtype_consonant,  0.72,  1},
    {EN_N,    280, 1700, 2600,  0.085, vtype_consonant,  0.72,  1},
    {EN_NG,   280, 2300, 3000,  0.080, vtype_consonant,  0.70,  1},

    // approximants
    {EN_L,    360,  980, 2480,  0.075, vtype_consonant,  0.82,  1},
    {EN_R,    460, 1190, 1580,  0.070, vtype_consonant,  0.80,  1},
    {EN_W,    290,  610, 2150,  0.065, vtype_consonant,  0.78,  1},
    {EN_Y,    260, 2100, 3000,  0.060, vtype_consonant,  0.76,  1},

    // terminator
    {0, 0, 0, 0, 0.0, 0, 0.0, 0}
};


// g2p grapheme to phoneme
// architecture espeak ng style rule matching
// each rule left context grapheme right context produces phones
// contexts use special letters like @ C and _
// rules derived from espeak ng and cmu frequency analysis

// placeholder code used in sc rule resolved to s and k in g2p loop
#define EN_SK  0xE070u

#define G2P_MAX_PH  6

typedef struct {
    const char *lctx;      // left context pattern null any
    const char *grapheme;  // grapheme to match
    const char *rctx;      // right context pattern null any
    uint32_t    phones[G2P_MAX_PH];
} G2PRule;

// rules sorted longer graphemes first more specific context first
// special context chars
// @ vowel letters a e i o u
// C consonant letter
// _ word boundary
// . any letter
// ! not

static const G2PRule g2p_rules[] = {

    // 4 letter sequences
    {NULL, "tion",  NULL,  {EN_SH, EN_AX, EN_N,  0,    0,    0}},
    {NULL, "sion",  "@",   {EN_ZH, EN_AX, EN_N,  0,    0,    0}},  // vision
    {NULL, "sion",  NULL,  {EN_SH, EN_AX, EN_N,  0,    0,    0}},  // pension
    {NULL, "tial",  NULL,  {EN_SH, EN_AX, EN_L,  0,    0,    0}},
    {NULL, "cial",  NULL,  {EN_SH, EN_AX, EN_L,  0,    0,    0}},
    {NULL, "ture",  NULL,  {EN_CH, EN_ER, 0,     0,    0,    0}},
    {NULL, "ight",  NULL,  {EN_AY1,EN_AY2,EN_T,  0,    0,    0}},
    {NULL, "ough",  NULL,  {EN_AH, EN_F,  0,     0,    0,    0}},  // rough default overridden below
    {NULL, "eigh",  NULL,  {EN_EY1,EN_EY2,0,     0,    0,    0}},  // eight
    {NULL, "ould",  NULL,  {EN_UH, EN_D,  0,     0,    0,    0}},  // could
    {NULL, "eous",  NULL,  {EN_IY, EN_AX, EN_S,  0,    0,    0}},
    {NULL, "ious",  NULL,  {EN_IY, EN_AX, EN_S,  0,    0,    0}},
    {NULL, "augh",  NULL,  {EN_AO, EN_F,  0,     0,    0,    0}},  // laugh
    {NULL, "ough",  "t",   {EN_AO, 0,     0,     0,    0,    0}},  // ought

    // 3 letter sequences
    {NULL, "tch",   NULL,  {EN_CH, 0,     0,     0,    0,    0}},
    {NULL, "dge",   NULL,  {EN_JH, 0,     0,     0,    0,    0}},
    {NULL, "igh",   NULL,  {EN_AY1,EN_AY2,0,     0,    0,    0}},
    {NULL, "ght",   NULL,  {EN_T,  0,     0,     0,    0,    0}},
    {NULL, "eau",   NULL,  {EN_OW1,EN_OW2,0,     0,    0,    0}},
    {NULL, "ire",   NULL,  {EN_AY1,EN_AY2,EN_R,  0,    0,    0}},
    {NULL, "ure",   NULL,  {EN_UH, EN_R,  0,     0,    0,    0}},
    {NULL, "ore",   NULL,  {EN_AO, EN_R,  0,     0,    0,    0}},
    {NULL, "are",   "_",   {EN_EH, EN_R,  0,     0,    0,    0}},
    {NULL, "air",   NULL,  {EN_EH, EN_R,  0,     0,    0,    0}},
    {NULL, "ear",   NULL,  {EN_IH, EN_R,  0,     0,    0,    0}},
    {NULL, "our",   NULL,  {EN_AW1,EN_AW2,EN_R,  0,    0,    0}},
    {NULL, "oul",   NULL,  {EN_UH, EN_L,  0,     0,    0,    0}},
    {NULL, "wor",   NULL,  {EN_ER, 0,     0,     0,    0,    0}},  // word work
    {NULL, "war",   NULL,  {EN_AO, EN_R,  0,     0,    0,    0}},
    {NULL, "ion",   NULL,  {EN_IH, EN_AX, EN_N,  0,    0,    0}},  // million fallback
    {NULL, "ous",   NULL,  {EN_AX, EN_S,  0,     0,    0,    0}},  // famous
    {NULL, "age",   "_",   {EN_IH, EN_JH, 0,     0,    0,    0}},  // village
    {NULL, "age",   NULL,  {EN_EY1,EN_EY2,EN_JH, 0,    0,    0}},  // cage
    {NULL, "ive",   "_",   {EN_IH, EN_V,  0,     0,    0,    0}},  // live adj
    {NULL, "ive",   NULL,  {EN_AY1,EN_AY2,EN_V,  0,    0,    0}},  // give
    {NULL, "ual",   NULL,  {EN_UW, EN_AX, EN_L,  0,    0,    0}},  // actual
    {NULL, "cia",   NULL,  {EN_SH, EN_AX, 0,     0,    0,    0}},
    {NULL, "tia",   NULL,  {EN_SH, EN_AX, 0,     0,    0,    0}},

    // consonant digraphs
    {NULL, "ch",    NULL,  {EN_CH, 0,     0,     0,    0,    0}},
    {NULL, "sh",    NULL,  {EN_SH, 0,     0,     0,    0,    0}},
    {NULL, "ph",    NULL,  {EN_F,  0,     0,     0,    0,    0}},
    {NULL, "wh",    NULL,  {EN_W,  0,     0,     0,    0,    0}},
    {NULL, "ck",    NULL,  {EN_K,  0,     0,     0,    0,    0}},
    {NULL, "ng",    NULL,  {EN_NG, 0,     0,     0,    0,    0}},
    {NULL, "nk",    NULL,  {EN_NG, EN_K,  0,     0,    0,    0}},
    {NULL, "gn",    "_",   {EN_N,  0,     0,     0,    0,    0}},   // gnat
    {NULL, "kn",    "_",   {EN_N,  0,     0,     0,    0,    0}},   // knee
    {NULL, "wr",    "_",   {EN_R,  0,     0,     0,    0,    0}},   // write
    {NULL, "dg",    NULL,  {EN_JH, 0,     0,     0,    0,    0}},
    {NULL, "gh",    NULL,  {0,     0,     0,     0,    0,    0}},   // silent
    {NULL, "qu",    NULL,  {EN_K,  EN_W,  0,     0,    0,    0}},
    {NULL, "sc",    "@",   {EN_S,  0,     0,     0,    0,    0}},   // science
    {NULL, "sc",    NULL,  {EN_SK, 0,     0,     0,    0,    0}},   // sc handled

    // vowel digraphs
    {NULL, "aw",    NULL,  {EN_AO, 0,     0,     0,    0,    0}},
    {NULL, "ow",    "_",   {EN_OW1,EN_OW2,0,     0,    0,    0}},  // know end of word
    {NULL, "ow",    "C",   {EN_OW1,EN_OW2,0,     0,    0,    0}},  // bowl
    {NULL, "ow",    NULL,  {EN_AW1,EN_AW2,0,     0,    0,    0}},  // cow
    {NULL, "oo",    NULL,  {EN_UW, 0,     0,     0,    0,    0}},
    {NULL, "ou",    NULL,  {EN_AW1,EN_AW2,0,     0,    0,    0}},
    {NULL, "oi",    NULL,  {EN_OI1,EN_OI2,0,     0,    0,    0}},
    {NULL, "oy",    NULL,  {EN_OI1,EN_OI2,0,     0,    0,    0}},
    {NULL, "ai",    NULL,  {EN_EY1,EN_EY2,0,     0,    0,    0}},
    {NULL, "ay",    NULL,  {EN_EY1,EN_EY2,0,     0,    0,    0}},
    {NULL, "ea",    "C",   {EN_EH, 0,     0,     0,    0,    0}},  // bread
    {NULL, "ea",    NULL,  {EN_IY, 0,     0,     0,    0,    0}},  // beat
    {NULL, "ee",    NULL,  {EN_IY, 0,     0,     0,    0,    0}},
    {NULL, "ie",    "_",   {EN_IY, 0,     0,     0,    0,    0}},  // pie
    {NULL, "ie",    NULL,  {EN_IH, 0,     0,     0,    0,    0}},  // field
    {NULL, "ue",    NULL,  {EN_UW, 0,     0,     0,    0,    0}},
    {NULL, "ui",    NULL,  {EN_UW, 0,     0,     0,    0,    0}},
    {NULL, "ew",    NULL,  {EN_UW, 0,     0,     0,    0,    0}},
    {NULL, "au",    NULL,  {EN_AO, 0,     0,     0,    0,    0}},
    {NULL, "eu",    NULL,  {EN_UW, 0,     0,     0,    0,    0}},

    // magic e vowel lengthening handled in code fallback rules provided
    {NULL, "ate",   NULL,  {EN_EY1,EN_EY2,EN_T,  0,    0,    0}},
    {NULL, "ame",   NULL,  {EN_EY1,EN_EY2,EN_M,  0,    0,    0}},
    {NULL, "ane",   NULL,  {EN_EY1,EN_EY2,EN_N,  0,    0,    0}},
    {NULL, "ake",   NULL,  {EN_EY1,EN_EY2,EN_K,  0,    0,    0}},
    {NULL, "aze",   NULL,  {EN_EY1,EN_EY2,EN_Z,  0,    0,    0}},
    {NULL, "ite",   NULL,  {EN_AY1,EN_AY2,EN_T,  0,    0,    0}},
    {NULL, "ile",   NULL,  {EN_AY1,EN_AY2,EN_L,  0,    0,    0}},
    {NULL, "ine",   NULL,  {EN_AY1,EN_AY2,EN_N,  0,    0,    0}},
    {NULL, "ise",   NULL,  {EN_AY1,EN_AY2,EN_Z,  0,    0,    0}},
    {NULL, "ize",   NULL,  {EN_AY1,EN_AY2,EN_Z,  0,    0,    0}},
    {NULL, "ife",   NULL,  {EN_AY1,EN_AY2,EN_F,  0,    0,    0}},
    {NULL, "ome",   NULL,  {EN_OW1,EN_OW2,EN_M,  0,    0,    0}},
    {NULL, "one",   NULL,  {EN_OW1,EN_OW2,EN_N,  0,    0,    0}},
    {NULL, "ope",   NULL,  {EN_OW1,EN_OW2,EN_P,  0,    0,    0}},
    {NULL, "oke",   NULL,  {EN_OW1,EN_OW2,EN_K,  0,    0,    0}},
    {NULL, "ole",   NULL,  {EN_OW1,EN_OW2,EN_L,  0,    0,    0}},
    {NULL, "ude",   NULL,  {EN_UW, EN_D,  0,     0,    0,    0}},
    {NULL, "une",   NULL,  {EN_UW, EN_N,  0,     0,    0,    0}},
    {NULL, "ute",   NULL,  {EN_UW, EN_T,  0,     0,    0,    0}},
    {NULL, "ube",   NULL,  {EN_UW, EN_B,  0,     0,    0,    0}},
    {NULL, "ule",   NULL,  {EN_UW, EN_L,  0,     0,    0,    0}},

    // common suffixes
    {NULL, "ed",    "_",   {EN_D,  0,     0,     0,    0,    0}},
    {NULL, "er",    "_",   {EN_ER, 0,     0,     0,    0,    0}},
    {NULL, "ing",   NULL,  {EN_IH, EN_NG, 0,     0,    0,    0}},
    {NULL, "est",   NULL,  {EN_AX, EN_S,  EN_T,  0,    0,    0}},
    {NULL, "ness",  NULL,  {EN_N,  EN_AX, EN_S,  0,    0,    0}},
    {NULL, "less",  NULL,  {EN_L,  EN_AX, EN_S,  0,    0,    0}},
    {NULL, "ful",   NULL,  {EN_F,  EN_AX, EN_L,  0,    0,    0}},
    {NULL, "ment",  NULL,  {EN_M,  EN_AX, EN_N,  EN_T, 0,    0}},
    {NULL, "ble",   NULL,  {EN_B,  EN_EL, 0,     0,    0,    0}},  // syllabic l
    {NULL, "ple",   NULL,  {EN_P,  EN_EL, 0,     0,    0,    0}},
    {NULL, "tle",   NULL,  {EN_T,  EN_EL, 0,     0,    0,    0}},
    {NULL, "dle",   NULL,  {EN_D,  EN_EL, 0,     0,    0,    0}},
    {NULL, "kle",   NULL,  {EN_K,  EN_EL, 0,     0,    0,    0}},
    {NULL, "gle",   NULL,  {EN_G,  EN_EL, 0,     0,    0,    0}},
    {NULL, "fle",   NULL,  {EN_F,  EN_EL, 0,     0,    0,    0}},
    {NULL, "sle",   NULL,  {EN_EL, 0,     0,     0,    0,    0}},  // hassle silent s
    {NULL, "ton",   "_",   {EN_T,  EN_EN, 0,     0,    0,    0}},  // button syllabic n
    {NULL, "ten",   "_",   {EN_T,  EN_EN, 0,     0,    0,    0}},  // kitten
    {NULL, "den",   "_",   {EN_D,  EN_EN, 0,     0,    0,    0}},  // hidden
    {NULL, "tten",  "_",   {EN_T,  EN_EN, 0,     0,    0,    0}},  // written
    {NULL, "le",    "_",   {EN_EL, 0,     0,     0,    0,    0}},  // gentle

    // single vowels default pronunciations
    {NULL, "a",     NULL,  {EN_AE, 0,     0,     0,    0,    0}},
    {NULL, "e",     NULL,  {EN_EH, 0,     0,     0,    0,    0}},
    {NULL, "i",     NULL,  {EN_IH, 0,     0,     0,    0,    0}},
    {NULL, "o",     NULL,  {EN_AO, 0,     0,     0,    0,    0}},
    {NULL, "u",     NULL,  {EN_AH, 0,     0,     0,    0,    0}},
    {NULL, "y",     "_",   {EN_IY, 0,     0,     0,    0,    0}},  // final y = iy
    {NULL, "y",     NULL,  {EN_IH, 0,     0,     0,    0,    0}},  // medial y

    // single consonants
    {NULL, "b",     NULL,  {EN_B,  0,     0,     0,    0,    0}},
    {NULL, "c",     NULL,  {EN_K,  0,     0,     0,    0,    0}},  // hard c default
    {NULL, "d",     NULL,  {EN_D,  0,     0,     0,    0,    0}},
    {NULL, "f",     NULL,  {EN_F,  0,     0,     0,    0,    0}},
    {NULL, "g",     NULL,  {EN_G,  0,     0,     0,    0,    0}},  // hard g default
    {NULL, "h",     NULL,  {EN_HH, 0,     0,     0,    0,    0}},
    {NULL, "j",     NULL,  {EN_JH, 0,     0,     0,    0,    0}},
    {NULL, "k",     NULL,  {EN_K,  0,     0,     0,    0,    0}},
    {NULL, "l",     NULL,  {EN_L,  0,     0,     0,    0,    0}},
    {NULL, "m",     NULL,  {EN_M,  0,     0,     0,    0,    0}},
    {NULL, "n",     NULL,  {EN_N,  0,     0,     0,    0,    0}},
    {NULL, "p",     NULL,  {EN_P,  0,     0,     0,    0,    0}},
    {NULL, "q",     NULL,  {EN_K,  0,     0,     0,    0,    0}},
    {NULL, "r",     NULL,  {EN_R,  0,     0,     0,    0,    0}},
    {NULL, "s",     NULL,  {EN_S,  0,     0,     0,    0,    0}},
    {NULL, "t",     NULL,  {EN_T,  0,     0,     0,    0,    0}},
    {NULL, "v",     NULL,  {EN_V,  0,     0,     0,    0,    0}},
    {NULL, "w",     NULL,  {EN_W,  0,     0,     0,    0,    0}},
    {NULL, "x",     NULL,  {EN_K,  EN_S,  0,     0,    0,    0}},
    {NULL, "z",     NULL,  {EN_Z,  0,     0,     0,    0,    0}},

    {NULL, NULL, NULL, {0,0,0,0,0,0}}
};


// workaround sc rule references en_sk which is handled as s k in loop

// context matching helpers

static int is_vowel_char(char c)
{
    return (c=='a'||c=='e'||c=='i'||c=='o'||c=='u');
}

static int match_ctx(const char *lw, int wlen, int pos, const char *pattern, int is_left)
{
    // is_left pattern checked ending at pos-1
    // is_right pattern checked starting at pos
    if (!pattern) return 1;

    if (is_left) {
        // check one char to the left
        char prev = (pos > 0) ? lw[pos - 1] : 0;
        if (strcmp(pattern, "@") == 0) return is_vowel_char(prev);
        if (strcmp(pattern, "C") == 0) return (prev >= 'a' && prev <= 'z') && !is_vowel_char(prev);
        if (strcmp(pattern, "_") == 0) return (pos == 0);
        if (strcmp(pattern, ".") == 0) return (prev >= 'a' && prev <= 'z');
        // literal
        int pl = (int)strlen(pattern);
        if (pos < pl) return 0;
        return (strncmp(lw + pos - pl, pattern, (size_t)pl) == 0);
    } else {
        // right context check at pos
        char next = (pos < wlen) ? lw[pos] : 0;
        if (strcmp(pattern, "@") == 0) return is_vowel_char(next);
        if (strcmp(pattern, "C") == 0) return (next >= 'a' && next <= 'z') && !is_vowel_char(next);
        if (strcmp(pattern, "_") == 0) return (pos >= wlen);
        if (strcmp(pattern, ".") == 0) return (next >= 'a' && next <= 'z');
        // literal
        int pl = (int)strlen(pattern);
        if (pos + pl > wlen) return 0;
        return (strncmp(lw + pos, pattern, (size_t)pl) == 0);
    }
}


// special pre pass rules handled before table lookup

// check if position has vowel consonant e pattern magic e
static int is_magic_e(const char *lw, int wlen, int pos)
{
    // pos is a vowel check pos consonant(s) e at end
    if (pos + 2 >= wlen) return 0;
    if (lw[wlen - 1] != 'e') return 0;
    // 1 or 2 consonants between pos+1 and wlen-2
    int ccount = 0;
    for (int k = pos + 1; k < wlen - 1; k++) {
        if (!is_vowel_char(lw[k])) ccount++;
        else return 0; // another vowel in between not magic e
    }
    return (ccount >= 1 && ccount <= 2);
}

// return tense vowel for magic e context
static uint32_t magic_e_vowel(char c)
{
    switch (c) {
        case 'a': return EN_EY;  // gate
        case 'e': return EN_IY;  // these
        case 'i': return EN_AY;  // bite
        case 'o': return EN_OW;  // bone
        case 'u': return EN_UW;  // cute
    }
    return 0;
}


// main g2p function returns number of phonemes written to out
static int en_grapheme_to_phonemes(const char *word, uint32_t *out, int max_out)
{
    int wlen = (int)strlen(word);
    if (!wlen || !out || max_out <= 0) return 0;

    char *lw = (char *)malloc((size_t)wlen + 1);
    if (!lw) return 0;
    for (int i = 0; i < wlen; i++) lw[i] = (char)tolower((unsigned char)word[i]);
    lw[wlen] = '\0';

    int oi = 0;
    int i  = 0;

    while (i < wlen && oi < max_out - 1) {

        // double consonant skip duplicate mapped not phonemic
        if (i + 1 < wlen && lw[i] == lw[i + 1] && !is_vowel_char(lw[i])) {
            // fall through to rule lookup then skip double
        }

        // soft c ce ci cy becomes s
        if (lw[i] == 'c' && i + 1 < wlen && (lw[i+1]=='e'||lw[i+1]=='i'||lw[i+1]=='y')) {
            out[oi++] = EN_S; i++; continue;
        }

        // soft g ge gi gy becomes dʒ heuristic for exceptions
        if (lw[i] == 'g' && i + 1 < wlen && (lw[i+1]=='e'||lw[i+1]=='i'||lw[i+1]=='y')) {
            out[oi++] = EN_JH; i++; continue;
        }

        // th voiced between vowels or at start of short function words
        if (lw[i] == 't' && i + 1 < wlen && lw[i+1] == 'h') {
            uint32_t ph = EN_TH;
            // voiced dh for short function words
            if (i == 0 && wlen <= 5) ph = EN_DH;
            else if (i > 0 && is_vowel_char(lw[i-1]) &&
                i + 2 < wlen && is_vowel_char(lw[i+2])) ph = EN_DH; // intervocalic
                out[oi++] = ph;
            i += 2; continue;
        }

        // ough special cases many variants
        if (i + 3 < wlen && lw[i]=='o'&&lw[i+1]=='u'&&lw[i+2]=='g'&&lw[i+3]=='h') {
            if (i + 4 < wlen && lw[i+4]=='t') { out[oi++]=EN_AO; }          // ought
            else if (i == 0 && wlen == 4)       { out[oi++]=EN_AH; out[oi++]=EN_F; } // enough
            else if (i > 0 && lw[i-1]=='r')     { out[oi++]=EN_UW; }         // through
            else if (i + 4 >= wlen)             { out[oi++]=EN_OW1; out[oi++]=EN_OW2; } // dough
            else                                { out[oi++]=EN_AW1; out[oi++]=EN_AW2; } // bough
            i += 4; continue;
        }

        // ed suffix rules t id or d
        if (lw[i]=='e' && i+1==wlen-1 && lw[i+1]=='d' && i > 0) {
            char prev = lw[i-1];
            if (prev=='t'||prev=='d') { out[oi++]=EN_IH; out[oi++]=EN_D; }
            else if (prev=='k'||prev=='p'||prev=='s'||prev=='f'||prev=='x'||prev=='c') { out[oi++]=EN_T; }
            else                     { out[oi++]=EN_D; }
            i += 2; continue;
        }

        // es suffix iz after sibilants z elsewhere
        if (lw[i]=='e' && i+1==wlen-1 && lw[i+1]=='s' && i > 0) {
            char prev = lw[i-1];
            if (prev=='s'||prev=='z'||prev=='x') { out[oi++]=EN_IH; out[oi++]=EN_Z; }
            else                                 { out[oi++]=EN_Z; }
            i += 2; continue;
        }

        // magic e single vowel handled here
        if (is_vowel_char(lw[i]) && !is_vowel_char(i > 0 ? lw[i-1] : 0)) {
            if (is_magic_e(lw, wlen, i)) {
                uint32_t tv = magic_e_vowel(lw[i]);
                if (tv) {
                    // for diphthongs push both halves
                    if (tv == EN_EY) { out[oi++]=EN_EY1; if(oi<max_out-1) out[oi++]=EN_EY2; }
                    else if (tv == EN_AY) { out[oi++]=EN_AY1; if(oi<max_out-1) out[oi++]=EN_AY2; }
                    else if (tv == EN_OW) { out[oi++]=EN_OW1; if(oi<max_out-1) out[oi++]=EN_OW2; }
                    else out[oi++] = tv;
                    i++; continue;
                }
            }
        }

        // silent trailing e no magic e match
        if (lw[i] == 'e' && i == wlen - 1 && wlen > 2) {
            i++; continue;
        }

        // table lookup longest matching rule
        int best_r = -1, best_len = 0;
        for (int r = 0; g2p_rules[r].grapheme != NULL; r++) {
            const G2PRule *rule = &g2p_rules[r];
            int gl = (int)strlen(rule->grapheme);
            if (gl <= best_len) continue;
            if (lw[i] != rule->grapheme[0]) continue;
            if (i + gl > wlen) continue;
            if (strncmp(lw + i, rule->grapheme, (size_t)gl) != 0) continue;
            if (!match_ctx(lw, wlen, i,       rule->lctx, 1)) continue;
            if (!match_ctx(lw, wlen, i + gl,  rule->rctx, 0)) continue;
            best_len = gl;
            best_r   = r;
        }

        if (best_r >= 0) {
            const G2PRule *rule = &g2p_rules[best_r];
            for (int p = 0; p < G2P_MAX_PH && rule->phones[p]; p++) {
                uint32_t ph = rule->phones[p];
                if (ph == EN_SK) {
                    if (oi < max_out - 1) out[oi++] = EN_S;
                    if (oi < max_out - 1) out[oi++] = EN_K;
                } else {
                    if (oi < max_out - 1) out[oi++] = ph;
                }
            }
            // skip doubled consonants after match
            if (best_len == 1 && !is_vowel_char(lw[i]) && i + 1 < wlen && lw[i+1] == lw[i])
                i += 2;
            else
                i += best_len;
            continue;
        }

        // absolute fallback map letters to phonemes unknown to schwa
        static const struct { char c; uint32_t ph; } fb[] = {
            {'a',EN_AE},{'e',EN_EH},{'i',EN_IH},{'o',EN_AO},{'u',EN_AH},
            {'b',EN_B},{'c',EN_K},{'d',EN_D},{'f',EN_F},{'g',EN_G},
            {'h',EN_HH},{'j',EN_JH},{'k',EN_K},{'l',EN_L},{'m',EN_M},
            {'n',EN_N},{'p',EN_P},{'q',EN_K},{'r',EN_R},{'s',EN_S},
            {'t',EN_T},{'v',EN_V},{'w',EN_W},{'y',EN_Y},{'z',EN_Z},
            {0,0}
        };
        int found = 0;
        for (int k = 0; fb[k].c; k++) {
            if (lw[i] == fb[k].c) { out[oi++] = fb[k].ph; found = 1; break; }
        }
        if (!found) out[oi++] = EN_AX; // unknown becomes schwa
        i++;
    }

    free(lw);
    return oi;
}


// punctuation pause table returns pause duration for codepoint
static double en_punctuation_pause(uint32_t cp)
{
    switch (cp) {
        case ' ':    return 0.08;
        case ',':    return 0.18;
        case ';':    return 0.24;
        case ':':    return 0.22;
        case '.':    return 0.38;
        case '!':    return 0.42;
        case '?':    return 0.40;
        case '-':    return 0.10;
        case 0x2013: return 0.18;   // en dash
        case 0x2014: return 0.28;   // em dash
        case '\n':   return 0.45;
        case '\r':   return 0.00;
        case '(':    return 0.06;
        case ')':    return 0.10;
        case '"':    return 0.04;
        case 0x201C: return 0.04;
        case 0x201D: return 0.04;
        case '\'':   return 0.00;
        default:     return 0.00;
    }
}


// number to words helpers

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
    while (*len + sl + 2 > *cap) { *cap *= 2; *buf = (char *)realloc(*buf, *cap); }
    memcpy(*buf + *len, s, sl);
    *len += sl;
    (*buf)[*len] = '\0';
}

static void en_num_to_words(long n, char **buf, size_t *cap, size_t *len)
{
    if (n < 0) { en_append(buf, cap, len, "negative "); n = -n; }
    if (n < 20) { en_append(buf, cap, len, en_ones[n]); en_append(buf, cap, len, " "); return; }
    if (n < 100) {
        en_append(buf, cap, len, en_tens[n / 10]);
        if (n % 10) { en_append(buf, cap, len, " "); en_append(buf, cap, len, en_ones[n % 10]); }
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
    if (n < 1000000000L) {
        en_num_to_words(n / 1000000, buf, cap, len);
        en_append(buf, cap, len, "million ");
        if (n % 1000000) en_num_to_words(n % 1000000, buf, cap, len);
        return;
    }
    char tmp[32]; snprintf(tmp, sizeof(tmp), "%ld", n);
    for (int k = 0; tmp[k]; k++) { en_append(buf, cap, len, en_ones[tmp[k]-'0']); en_append(buf, cap, len, " "); }
}

static char *en_expand_input(const char *in)
{
    size_t cap = strlen(in) * 12 + 128;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t oi = 0; out[0] = '\0';

    for (size_t i = 0; in[i]; ) {
        unsigned char c = (unsigned char)in[i];
        if (c >= '0' && c <= '9') {
            long num = 0;
            while ((unsigned char)in[i] >= '0' && (unsigned char)in[i] <= '9')
            { num = num * 10 + (in[i] - '0'); i++; }
            en_num_to_words(num, &out, &cap, &oi);
        } else {
            if (oi + 8 >= cap) { cap *= 2; out = (char *)realloc(out, cap); }
            if (in[i] == ' ' || in[i] == '\t') {
                if (oi > 0 && out[oi-1] != ' ') out[oi++] = ' ';
                i++;
            } else {
                out[oi++] = in[i++];
            }
            out[oi] = '\0';
        }
    }
    while (oi > 0 && out[oi-1] == ' ') out[--oi] = '\0';
    return out;
}


// phoneme lookup and classification

static PhonemeDef *en_find_phoneme(uint32_t code)
{
    for (int i = 0; en_phonemes[i].code != 0; i++)
        if (en_phonemes[i].code == code) return &en_phonemes[i];
        return NULL;
}

static inline int en_is_vowel(uint32_t c)
{
    // all codes in the vowel ranges
    return (c >= EN_AE && c <= EN_AX) || (c >= EN_EY1 && c <= EN_AY2) ||
    c == EN_EL || c == EN_EN || c == EN_EM;
}

static inline int en_is_diphthong_onset(uint32_t c)
{
    return c==EN_EY1 || c==EN_AW1 || c==EN_OW1 || c==EN_OI1 || c==EN_AY1;
}

static inline int en_is_nasal(uint32_t c)
{
    return c==EN_M || c==EN_N || c==EN_NG || c==EN_EN || c==EN_EM;
}

static inline int en_is_stop(uint32_t c)
{
    return (c >= EN_P && c <= EN_G) || c==EN_CH || c==EN_JH;
}

static inline int en_is_fricative(uint32_t c)
{
    return (c >= EN_F && c <= EN_HH);
}

static inline int en_is_sonorant(uint32_t c)
{
    return en_is_nasal(c) || c==EN_L || c==EN_R || c==EN_W || c==EN_Y ||
    c==EN_EL || c==EN_EN || c==EN_EM;
}

static inline double en_clamp(double x, double lo, double hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}


// coarticulation
// goals formant transitions vowel duration nasal assimilation r retroflexion l darkening hh vowel borrowing
// limits formants plus minus fifteen percent durations plus minus twenty five percent

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

    double dur = cur->duration;
    double amp = cur->amp;
    int f1 = cur->f1, f2 = cur->f2, f3 = cur->f3;

    if (en_is_vowel(cur_code)) {

        // pre voiced lengthening before voiced consonant
        if (next && !en_is_vowel(next_code) && next->is_voiced)
            dur *= 1.18;
        else if (next && !en_is_vowel(next_code) && !next->is_voiced)
            dur *= 0.88;

        // r colouring pull f3 down when flanked by r
        if ((prev_code == EN_R || next_code == EN_R) && f3 > 0)
            f3 = (int)(f3 * 0.88);

        // l coda darkening lower f2
        if (next_code == EN_L)
            f2 = (int)(f2 * 0.93);

        // nasal coarticulation slight f1 drop before nasals
        if (next && en_is_nasal(next_code))
            f1 = (int)(f1 * 0.92);

    } else if (cur_code == EN_HH) {
        // h borrows formant pattern of following vowel
        if (next && en_is_vowel(next_code)) {
            f1 = next->f1;
            f2 = next->f2;
            f3 = next->f3;
            amp = (float)(next->amp * 0.55);
        }

    } else if (en_is_nasal(cur_code)) {
        // nasals adjacent to vowels amplitude boost
        if ((prev && en_is_vowel(prev_code)) || (next && en_is_vowel(next_code)))
            amp *= 1.08;

    } else if (cur_code == EN_L) {
        // dark l in coda lower f2
        if (!next || !en_is_vowel(next_code))
            f2 = (int)(f2 * 0.80);

    } else if (cur_code == EN_R) {
        // rhotic ensure f3 consistent
        f3 = (f3 > 0) ? f3 : 1580;

    } else if (en_is_stop(cur_code)) {
        // amplitude lift when between vowels
        if (prev && en_is_vowel(prev_code) && next && en_is_vowel(next_code))
            amp *= 1.06;
    }

    // hard clamp
    dur = en_clamp(dur, 0.030, 0.500);
    amp = en_clamp(amp, 0.10,  1.50);

    out_def->f1       = f1;
    out_def->f2       = f2;
    out_def->f3       = f3;
    out_def->duration = dur;
    out_def->amp      = (float)amp;
    out_def->code     = cur_code;
}


// phoneme name table debug

static const char *en_phoneme_name(uint32_t code)
{
    switch (code) {
        case EN_IY:  return "/iː/ fleece";
        case EN_UW:  return "/uː/ goose";
        case EN_IH:  return "/ɪ/ kit";
        case EN_EH:  return "/ɛ/ dress";
        case EN_AE:  return "/æ/ trap";
        case EN_AH:  return "/ʌ/ strut";
        case EN_AO:  return "/ɔː/ thought";
        case EN_AA:  return "/ɑ/ lot";
        case EN_UH:  return "/ʊ/ foot";
        case EN_ER:  return "/ɝ/ nurse";
        case EN_AX:  return "/ə/ schwa";
        case EN_EY1: return "/eɪ/ face onset";
        case EN_EY2: return "/eɪ/ face glide";
        case EN_AY1: return "/aɪ/ price onset";
        case EN_AY2: return "/aɪ/ price glide";
        case EN_AW1: return "/aʊ/ mouth onset";
        case EN_AW2: return "/aʊ/ mouth glide";
        case EN_OW1: return "/oʊ/ goat onset";
        case EN_OW2: return "/oʊ/ goat glide";
        case EN_OI1: return "/ɔɪ/ choice onset";
        case EN_OI2: return "/ɔɪ/ choice glide";
        case EN_EL:  return "/l̩/ syllabic-l";
        case EN_EN:  return "/n̩/ syllabic-n";
        case EN_EM:  return "/m̩/ syllabic-m";
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
