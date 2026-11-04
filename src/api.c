/* 公開 API  SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ct_internal.h"

#if defined(_MSC_VER)
#  define CT_THREAD_LOCAL __declspec(thread)
#else
#  define CT_THREAD_LOCAL _Thread_local
#endif

#define MAX_INPUT_BYTES (64 * 1024)
#define OUT_GAIN 1.6f    /* 合成器出力 → フルスケール（経験的に調整） */

static CT_THREAD_LOCAL int g_err_pos = -1;

/*                f0   fscale  oq    tilt breath jitter f0range b1add */
static const CtVoice VOICES[CT_VOICE_COUNT] = {
    /* F1    */ { 210, 1.15f, 0.60f, 10, 38, 0.010f, 1.00f, 20 },
    /* M1    */ { 115, 1.00f, 0.50f,  6, 32, 0.008f, 1.00f,  0 },
    /* CHILD */ { 290, 1.28f, 0.62f, 12, 38, 0.010f, 1.10f, 40 },
    /* ROBOT */ { 140, 1.05f, 0.40f,  0,  0, 0.000f, 0.60f,  0 },
};

void CT_CALL CTalk_DefaultParams(CTalkParams* p)
{
    if (!p) return;
    p->voice = CT_VOICE_F1;
    p->speed = 100;
    p->pitch = 100;
    p->accent = 100;
    p->volume = 100;
    p->sample_rate = 0;
}

static int check_params(const CTalkParams* p)
{
    if (p->voice < 0 || p->voice >= CT_VOICE_COUNT) return 0;
    if (p->speed < 50 || p->speed > 300) return 0;
    if (p->pitch < 50 || p->pitch > 200) return 0;
    if (p->accent < 0 || p->accent > 200) return 0;
    if (p->volume < 0 || p->volume > 200) return 0;
    if (p->sample_rate != 0 && (p->sample_rate < 8000 || p->sample_rate > 48000)) return 0;
    return 1;
}

/* なめらかなリミッタ */
static short to_pcm(float x)
{
    float a = fabsf(x), y;
    if (a > 0.8f) {
        float over = (a - 0.8f) / 0.2f;
        a = 0.8f + 0.2f * (over / (1.0f + over));
    }
    y = (x < 0 ? -a : a) * 32767.0f;
    if (y > 32767.0f) y = 32767.0f;
    if (y < -32768.0f) y = -32768.0f;
    return (short)lrintf(y);
}

static int synth(const char* koe, const CTalkParams* params, short** pcm, int* samples, int* fs_out)
{
    CTalkParams p;
    CtVoice v;
    CtUtterance u;
    CtPlan plan;
    CtFrame* fr = NULL;
    float* wave = NULL;
    short* out;
    int nfr = 0, ns = 0, fs, err, i, pos = -1;
    float gain;

    g_err_pos = -1;
    if (!koe || !pcm || !samples) return CT_ERR_NULL;
    *pcm = NULL;
    *samples = 0;
    if (params) p = *params; else CTalk_DefaultParams(&p);
    if (!check_params(&p)) return CT_ERR_PARAM;
    if (strlen(koe) > MAX_INPUT_BYTES) return CT_ERR_TOOLONG;

    fs = p.sample_rate ? p.sample_rate : 16000;
    v = VOICES[p.voice];
    v.f0 *= p.pitch / 100.0f;

    err = ct_parse(koe, &u, &pos);
    if (err) { g_err_pos = pos; return err; }
    err = ct_build_plan(&u, &v, (float)p.speed, (float)p.accent, &plan);
    ct_utterance_free(&u);
    if (err) return err;
    err = ct_build_frames(&plan, &v, (float)fs, &fr, &nfr);
    ct_plan_free(&plan);
    if (err) return err;
    err = ct_klatt_render(fr, nfr, &v, fs, &wave, &ns);
    free(fr);
    if (err) return err;

    out = (short*)malloc((size_t)(ns > 0 ? ns : 1) * sizeof *out);
    if (!out) { free(wave); return CT_ERR_MEMORY; }
    gain = OUT_GAIN * p.volume / 100.0f;
    for (i = 0; i < ns; i++) out[i] = to_pcm(wave[i] * gain);
    free(wave);

    *pcm = out;
    *samples = ns;
    if (fs_out) *fs_out = fs;
    return CT_OK;
}

int CT_CALL CTalk_SynthePCM(const char* koe_utf8, const CTalkParams* params,
                            short** pcm, int* samples, int* sample_rate)
{
    return synth(koe_utf8, params, pcm, samples, sample_rate);
}

unsigned char* CT_CALL CTalk_Synthe(const char* koe_utf8, const CTalkParams* params, int* size, int* err)
{
    short* pcm = NULL;
    int n = 0, fs = 16000, e;
    unsigned char* wav = NULL;

    if (size) *size = 0;
    if (!size) {
        e = CT_ERR_NULL;
    } else {
        e = synth(koe_utf8, params, &pcm, &n, &fs);
        if (!e) {
            wav = ct_wav_build(pcm, n, fs, size);
            if (!wav) e = CT_ERR_MEMORY;
        }
    }
    free(pcm);
    if (err) *err = e;
    return wav;
}

void CT_CALL CTalk_FreeWave(void* buf)
{
    free(buf);
}

const char* CT_CALL CTalk_ErrorString(int err)
{
    switch (err) {
    case CT_OK:          return "ok";
    case CT_ERR_NULL:    return "null argument";
    case CT_ERR_UTF8:    return "invalid UTF-8";
    case CT_ERR_CHAR:    return "character not allowed in koe string";
    case CT_ERR_ACCENT:  return "misplaced accent mark (')";
    case CT_ERR_LONG:    return "misplaced long vowel mark";
    case CT_ERR_DEVOICE: return "misplaced devoicing mark (_)";
    case CT_ERR_EMPTY:   return "nothing to speak";
    case CT_ERR_PARAM:   return "parameter out of range";
    case CT_ERR_TOOLONG: return "koe string too long";
    case CT_ERR_MEMORY:  return "out of memory";
    default:             return "unknown error";
    }
}

int CT_CALL CTalk_ErrorPos(void)
{
    return g_err_pos;
}

const char* CT_CALL CTalk_Version(void)
{
    return CTALK_VERSION;
}
