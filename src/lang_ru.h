#pragma once

/* * russian phoneme table and utilities
 * fields: codepoint, f1, f2, f3, duration, type, amplitude, is_voiced
 * formants adapted from peterson & barney (1952) and bondarko (1998)
 * input is normalized to uppercase; soft sign (0x042c) emits silence
 */

#include "tts_synth.h"

/* russian phoneme definitions */
static PhonemeDef ru_phonemes[] = {

    /* vowels */
    /* a: open central vowel */
    {0x0410, 700, 1220, 2600, 0.15, vtype_vowel,     1.00, 1},
    /* ye: mid front vowel */
    {0x0415, 500, 1700, 2500, 0.13, vtype_vowel,     0.95, 1},
    /* yo: mid front vowel with rounding */
    {0x0401, 500, 1700, 2500, 0.13, vtype_vowel,     0.95, 1},
    /* i: close front vowel */
    {0x0418, 300, 2200, 2950, 0.12, vtype_vowel,     0.90, 1},
    /* o: mid back rounded vowel */
    {0x041E, 500,  900, 2300, 0.14, vtype_vowel,     0.95, 1},
    /* u: close back rounded vowel */
    {0x0423, 300,  870, 2250, 0.14, vtype_vowel,     0.90, 1},
    /* yery: high central vowel */
    {0x042B, 400, 1400, 2500, 0.13, vtype_vowel,     0.88, 1},
    /* e: open front vowel */
    {0x042D, 550, 1700, 2400, 0.13, vtype_vowel,     0.90, 1},
    /* yu: rounded u with front coloring */
    {0x042E, 300,  900, 2200, 0.13, vtype_vowel,     0.90, 1},
    /* ya: palatalized a */
    {0x042F, 600, 1500, 2550, 0.14, vtype_vowel,     0.92, 1},

    /* voiced stops */
    /* b: voiced bilabial stop */
    {0x0411, 250,  700,    0, 0.14, vtype_consonant, 0.55, 1},
    /* g: voiced velar stop */
    {0x0413, 200,  600,    0, 0.13, vtype_consonant, 0.50, 1},
    /* d: voiced alveolar stop */
    {0x0414, 250,  500,    0, 0.14, vtype_consonant, 0.52, 1},

    /* voiced fricatives and approximants */
    /* v: voiced labiodental fricative */
    {0x0412, 800, 2000,    0, 0.14, vtype_consonant, 0.50, 1},
    /* zh: voiced postalveolar fricative */
    {0x0416, 2800, 3500,   0, 0.14, vtype_fricative, 0.38, 1},
    /* z: voiced alveolar sibilant */
    {0x0417, 2200, 3500,   0, 0.15, vtype_fricative, 0.38, 1},

    /* sonorants */
    /* j: palatal approximant */
    {0x0419,  300, 2000,   0, 0.11, vtype_consonant, 0.48, 1},
    /* l: lateral approximant */
    {0x041B,  400,  900,   0, 0.14, vtype_consonant, 0.55, 1},
    /* m: bilabial nasal */
    {0x041C,  300,  900,   0, 0.14, vtype_consonant, 0.58, 1},
    /* n: alveolar nasal */
    {0x041D,  300, 1000,   0, 0.14, vtype_consonant, 0.58, 1},
    /* r: trill or tap */
    {0x0420,  500, 1500,   0, 0.14, vtype_consonant, 0.52, 1},

    /* voiceless stops */
    /* p: voiceless bilabial stop */
    {0x041F,  200,  600,   0, 0.10, vtype_stop,      0.45, 0},
    /* t: voiceless alveolar stop */
    {0x0422,  300,  900,   0, 0.10, vtype_stop,      0.45, 0},
    /* k: voiceless velar stop */
    {0x041A,  800, 1500,   0, 0.10, vtype_stop,      0.42, 0},

    /* voiceless fricatives */
    /* f: voiceless labiodental fricative */
    {0x0424, 2200, 4000,   0, 0.14, vtype_fricative, 0.32, 0},
    /* s: voiceless alveolar sibilant */
    {0x0421, 4000, 6000,   0, 0.16, vtype_fricative, 0.30, 0},
    /* kh: voiceless velar fricative */
    {0x0425, 2000, 3500,   0, 0.15, vtype_fricative, 0.30, 0},
    /* sh: voiceless postalveolar sibilant */
    {0x0428, 2800, 4000,   0, 0.16, vtype_fricative, 0.35, 0},
    /* shch: long postalveolar sibilant */
    {0x0429, 2800, 4200,   0, 0.17, vtype_fricative, 0.35, 0},

    /* affricates */
    /* ts: voiceless alveolar affricate */
    {0x0426, 2000, 4000,   0, 0.11, vtype_stop,      0.38, 0},
    /* ch: voiceless postalveolar affricate */
    {0x0427, 2200, 4000,   0, 0.11, vtype_stop,      0.38, 0},

    /* non-phonemic signs */
    /* hard sign: filler pause */
    {0x042A,    0,    0,   0, 0.03, vtype_silence,   0.00, 0},
    /* soft sign: no sound */
    {0x042C,    0,    0,   0, 0.00, vtype_silence,   0.00, 0},

    /* array terminator */
    {0, 0, 0, 0, 0, 0, 0, 0}
};

/* convert lowercase cyrillic to uppercase codepoints */
static uint32_t ru_normalize_upper(uint32_t cp)
{
    if (cp >= 0x0430 && cp <= 0x044F) return cp - 0x20;
    if (cp == 0x0451) return 0x0401;
    return cp;
}

/* return pause duration in seconds for punctuation characters */
static double ru_punctuation_pause(uint32_t cp)
{
    if (cp == 32)              return 0.08; /* space */
        if (cp == 44)              return 0.15; /* comma */
            if (cp == 46)              return 0.28; /* period */
                if (cp == 33 || cp == 63)  return 0.32; /* ! ? */
                    if (cp == 59 || cp == 58)  return 0.20; /* ; : */
                        if (cp == 10)              return 0.35; /* newline */
                            if (cp == 45)              return 0.07; /* hyphen */
                                return 0.0;
}

/* expand ascii digits to russian word equivalents. caller must free memory. */
static char *ru_expand_input(const char *in)
{
    static const char *dw[10] = {
        "ноль","один","два","три","четыре",
        "пять","шесть","семь","восемь","девять"
    };
    size_t inl = strlen(in), outcap = (inl + 1) * 6 + 16;
    char *out = malloc(outcap);
    if (!out) return NULL;
    out[0] = 0;
    size_t oi = 0;
    for (size_t i = 0; i < inl; ) {
        unsigned char c = (unsigned char)in[i];
        if (c < 0x80) {
            if (c >= '0' && c <= '9') {
                const char *w = dw[c - '0'];
                size_t wl = strlen(w);
                if (oi + wl + 2 >= outcap) {
                    outcap *= 2;
                    out = realloc(out, outcap);
                }
                memcpy(out + oi, w, wl);  oi += wl;
                out[oi++] = ' ';  out[oi] = 0;
                i++;  continue;
            }
            if (oi + 2 >= outcap) {
                outcap *= 2;
                out = realloc(out, outcap);
            }
            out[oi++] = in[i++];  out[oi] = 0;
        } else {
            int bytes = 1;
            if      ((c & 0xE0) == 0xC0) bytes = 2;
            else if ((c & 0xF0) == 0xE0) bytes = 3;
            else if ((c & 0xF8) == 0xF0) bytes = 4;
            if (i + bytes > inl) bytes = (int)(inl - i);
            if (oi + bytes + 2 >= outcap) {
                outcap *= 2;
                out = realloc(out, outcap);
            }
            memcpy(out + oi, in + i, bytes);  oi += bytes;  out[oi] = 0;
            i += bytes;
        }
    }
    while (oi > 0 && out[oi - 1] == ' ') { out[--oi] = 0; }
    return out;
}
