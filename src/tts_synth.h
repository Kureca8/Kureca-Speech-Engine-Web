#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define SAMPLE_RATE 16000
#define TWO_PI (2.0 * M_PI)

/* phoneme class definitions */
typedef enum {
    vtype_vowel      = 0,   /* voiced vowel: 3-formant synthesis */
    vtype_consonant  = 1,   /* voiced/unvoiced consonant: low formants + lp */
    vtype_fricative  = 2,   /* noisy fricative: bandpass filtered noise */
    vtype_stop       = 3,   /* plosive: burst then silence/voicing */
    vtype_silence    = 4    /* pause / silent segment */
} PhType;

/* entry in a language phoneme table */
typedef struct {
    uint32_t code;
    double   f1, f2, f3, duration;
    PhType   type;
    double   amp;
    int      is_voiced;
} PhonemeDef;

/* runtime state for one synthesized segment */
typedef struct {
    double sampleRate;
    int    totalSamples, currentSample;
    PhType type;
    int    is_voiced;
    double amplitude;
    double f[3];

    /* biquad coefficients for up to 3 bandpass formant filters */
    double b0[3], b1[3], b2[3], a1[3], a2[3];

    /* lowpass biquad for smoothing fricatives/consonants */
    double lp_b0, lp_b1, lp_b2, lp_a1, lp_a2;
    double lp_x1, lp_x2, lp_y1, lp_y2;

    /* biquad state per formant */
    double x1[3], x2[3], y1[3], y2[3];

    /* glottal source state */
    double phase_f0;
    double pitch;
    double glottal_norm;
    int    glottal_max_h;

    /* noise / burst state */
    double noise_hp;
    int    burstRemaining;

    /* amplitude envelope state */
    int attack_samples, release_samples;

    /* debug info */
    uint32_t dbg_code;
} FormantData;

/* ordered sequence of formant segments */
typedef struct {
    FormantData *seq;
    int seqLen, currentIndex;
} TTSSeq;

/* global speed multiplier */
static double READ_SPEED = 1.0;

/* bandpass filter coefficient calculation */
static void init_bandpass(double fs, double f0, double Q,
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

/* lowpass filter coefficient calculation */
static void init_lowpass(double fs, double fc,
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

/* calculate current envelope amplitude */
static double envelope_amp(FormantData *d)
{
    int n    = d->currentSample;
    int tail = d->totalSamples - n;
    if (n    < d->attack_samples)  return (double)n    / (double)(d->attack_samples  + 1);
    if (tail < d->release_samples) return (double)tail / (double)(d->release_samples + 1);
    return 1.0;
}

/* generate periodic glottal source signal */
static double glottal_source(FormantData *d)
{
    double inc = TWO_PI * d->pitch / d->sampleRate;
    d->phase_f0 += inc;
    if (d->phase_f0 > TWO_PI) d->phase_f0 -= TWO_PI;
    double src = 0.0;
    for (int h = 1; h <= d->glottal_max_h; h++) src += sin(d->phase_f0 * h) / (double)h;
    return (src / d->glottal_norm) * 0.6;
}

/* process sample through specific biquad filter */
static double apply_biquad(FormantData *d, int idx, double x)
{
    double y = d->b0[idx]*x + d->b1[idx]*d->x1[idx] + d->b2[idx]*d->x2[idx]
    - d->a1[idx]*d->y1[idx] - d->a2[idx]*d->y2[idx];
    d->x2[idx] = d->x1[idx];  d->x1[idx] = x;
    d->y2[idx] = d->y1[idx];  d->y1[idx] = y;
    return y;
}

/* process sample through lowpass filter */
static double apply_lp(FormantData *d, double x)
{
    double y = d->lp_b0*x + d->lp_b1*d->lp_x1 + d->lp_b2*d->lp_x2
    - d->lp_a1*d->lp_y1 - d->lp_a2*d->lp_y2;
    d->lp_x2 = d->lp_x1;  d->lp_x1 = x;
    d->lp_y2 = d->lp_y1;  d->lp_y1 = y;
    return y;
}

/* restrict signal range with soft clipping */
static double softclip(double x)
{
    if (x >  1.0) return  1.0 - 1.0 / (1.0 + (x  - 1.0) * 3.0);
    if (x < -1.0) return -1.0 + 1.0 / (1.0 + (-x - 1.0) * 3.0);
    return x;
}

/* initialize runtime formant data from phoneme definition */
static void setup_formant(FormantData *d, PhonemeDef *pd,
                          uint32_t dbg_code, double duration_override)
{
    memset(d, 0, sizeof(FormantData));
    d->sampleRate  = SAMPLE_RATE;
    d->type        = pd->type;
    d->is_voiced   = pd->is_voiced;
    d->amplitude   = pd->amp;
    d->f[0] = pd->f1;  d->f[1] = pd->f2;  d->f[2] = pd->f3;
    d->dbg_code    = dbg_code;

    double dur = (duration_override > 0.0) ? duration_override : pd->duration;
    d->totalSamples = (int)(dur * SAMPLE_RATE / READ_SPEED);
    if (d->totalSamples < 2) d->totalSamples = 2;

    /* calculate pitch with slight randomization for natural sound */
    d->pitch = 90.0 + (double)(rand() % 41);
    {
        int max_h = (int)(d->sampleRate / (2.0 * d->pitch));
        if (max_h < 1) max_h = 1;
        if (max_h > 40) max_h = 40;
        double norm = 0.0;
        for (int h = 1; h <= max_h; h++) norm += 1.0 / (double)h;
        d->glottal_max_h = max_h;
        d->glottal_norm  = (norm > 0.0) ? norm : 1.0;
    }

    d->phase_f0 = 0.0;  d->noise_hp = 0.0;
    d->burstRemaining = (d->type == vtype_stop) ? (int)(0.018 * SAMPLE_RATE) : 0;

    /* configure envelope windows */
    d->attack_samples  = (int)(d->totalSamples * 0.14);
    if (d->attack_samples < 3) d->attack_samples = 3;
    d->release_samples = (int)(d->totalSamples * 0.22);
    {
        int min_rel = (int)(0.018 * SAMPLE_RATE);
        if (d->release_samples < min_rel) d->release_samples = min_rel;
    }
    if (d->release_samples > d->totalSamples / 2) d->release_samples = d->totalSamples / 2;
    if (d->release_samples < 2) d->release_samples = 2;

    double q_scale = d->pitch / 130.0;
    if (q_scale > 1.0) q_scale = 1.0;
    if (q_scale < 0.5) q_scale = 0.5;

    /* initialize specific filter configurations based on phoneme type */
    if (d->type == vtype_vowel) {
        init_bandpass(SAMPLE_RATE, d->f[0], 7.0*q_scale+1.0, &d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
        init_bandpass(SAMPLE_RATE, d->f[1], 9.0*q_scale+1.0, &d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
        init_bandpass(SAMPLE_RATE, d->f[2], 12.0*q_scale+1.0, &d->b0[2],&d->b1[2],&d->b2[2],&d->a1[2],&d->a2[2]);
    } else if (d->type == vtype_fricative) {
        double fc1 = (d->f[0] > 0.0) ? d->f[0] : 3000.0;
        double fc2 = (d->f[1] > 0.0) ? d->f[1] : fc1 * 1.3;
        init_bandpass(SAMPLE_RATE, fc1, 2.5, &d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
        init_bandpass(SAMPLE_RATE, fc2, 2.0, &d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
        double lp_fc = fc1 < 3500.0 ? fc1 * 0.80 : 2800.0;
        init_lowpass(SAMPLE_RATE, lp_fc, &d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
    } else if (d->type == vtype_consonant) {
        double fc1 = (d->f[0] > 0.0) ? d->f[0] : 400.0;
        double fc2 = (d->f[1] > 0.0) ? d->f[1] : 1200.0;
        init_bandpass(SAMPLE_RATE, fc1, 5.0*q_scale+1.0, &d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
        init_bandpass(SAMPLE_RATE, fc2, 6.0*q_scale+1.0, &d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
        init_lowpass(SAMPLE_RATE, 3000.0, &d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
    } else if (d->type == vtype_stop) {
        double fc1 = (d->f[0] > 0.0) ? d->f[0] : 600.0;
        double fc2 = (d->f[1] > 0.0) ? d->f[1] : 1800.0;
        init_bandpass(SAMPLE_RATE, fc1, 3.5, &d->b0[0],&d->b1[0],&d->b2[0],&d->a1[0],&d->a2[0]);
        init_bandpass(SAMPLE_RATE, fc2, 3.0, &d->b0[1],&d->b1[1],&d->b2[1],&d->a1[1],&d->a2[1]);
        init_lowpass(SAMPLE_RATE, 2500.0, &d->lp_b0,&d->lp_b1,&d->lp_b2,&d->lp_a1,&d->lp_a2);
    }
}

/* generate the next sample in the sequence */
static float generate_sample(TTSSeq *tts)
{
    if (!tts || tts->currentIndex >= tts->seqLen) return 0.0f;
    FormantData *d = &tts->seq[tts->currentIndex];

    while (d->currentSample >= d->totalSamples) {
        tts->currentIndex++;
        if (tts->currentIndex >= tts->seqLen) return 0.0f;
        d = &tts->seq[tts->currentIndex];
    }

    double env = envelope_amp(d);
    double s = 0.0;

    if (d->type == vtype_silence) {
        s = 0.0;
    } else if (d->type == vtype_vowel) {
        double src = glottal_source(d);
        double y0 = apply_biquad(d, 0, src);
        double y1 = apply_biquad(d, 1, src);
        double y2 = apply_biquad(d, 2, src);
        s = (0.5*y0 + 1.0*y1 + 0.8*y2) * d->amplitude * env * 0.9;
    } else if (d->type == vtype_consonant) {
        double src;
        if (d->is_voiced)
            src = glottal_source(d)*0.55 + ((double)rand()/(double)RAND_MAX*2.0-1.0)*0.02;
        else
            src = ((double)rand()/(double)RAND_MAX*2.0-1.0)*0.10;
        double y0 = apply_biquad(d, 0, src);
        double y1 = apply_biquad(d, 1, src);
        s = apply_lp(d, y0*0.6 + y1*0.4) * d->amplitude * env;
    } else if (d->type == vtype_fricative) {
        double n = ((double)rand()/(double)RAND_MAX*2.0-1.0);
        double hp = n - 0.88*d->noise_hp; d->noise_hp = n;
        double y0 = apply_biquad(d, 0, hp);
        double y1 = apply_biquad(d, 1, hp);
        double mix = y0*0.6 + y1*0.4;
        if (d->is_voiced) mix = mix*0.5 + glottal_source(d)*0.35;
        s = apply_lp(d, mix) * d->amplitude * env;
    } else if (d->type == vtype_stop) {
        if (d->burstRemaining > 0) {
            double noise = ((double)rand()/(double)RAND_MAX*2.0-1.0);
            double n_hp = noise - 0.85*d->noise_hp; d->noise_hp = noise;
            double y0 = apply_biquad(d, 0, n_hp);
            double y1 = apply_biquad(d, 1, n_hp);
            double benv = (double)d->burstRemaining / (0.018 * d->sampleRate);
            s = apply_lp(d, (y0*0.5 + y1*0.5) * benv * 0.6);
            d->burstRemaining--;
        } else {
            s = d->is_voiced ? glottal_source(d)*0.25*d->amplitude*env : 0.0;
        }
    }

    if (!isfinite(s)) s = 0.0;
    d->currentSample++;
    return (float)softclip(s * 1.8);
}

/* reset sequence state to beginning */
static void reset_seq(TTSSeq *tts)
{
    tts->currentIndex = 0;
    for (int i = 0; i < tts->seqLen; i++) {
        FormantData *d = &tts->seq[i];
        d->currentSample = 0; d->phase_f0 = 0.0; d->noise_hp = 0.0;
        d->lp_x1 = d->lp_x2 = d->lp_y1 = d->lp_y2 = 0.0;
        d->burstRemaining = (d->type == vtype_stop) ? (int)(0.018*SAMPLE_RATE) : 0;
        for (int j = 0; j < 3; j++) d->x1[j]=d->x2[j]=d->y1[j]=d->y2[j]=0.0;
    }
}

/* free memory allocated for tts sequence */
static void free_tts(TTSSeq *t)
{
    if (!t) return;
    if (t->seq) free(t->seq);
    free(t);
}
