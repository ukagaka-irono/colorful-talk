/* 音素セグメント → 合成器パラメータの目標値  SPDX-License-Identifier: MIT */
#include <math.h>
#include <string.h>
#include "ct_internal.h"

/* 成人男性基準の母音フォルマント: F1 F2 F3 B1 B2 B3 */
static const float VF[6][6] = {
    { 500, 1500, 2500, 80, 100, 140 },  /* (なし) */
    { 750, 1250, 2600, 90, 100, 140 },  /* a */
    { 290, 2250, 3000, 60, 100, 150 },  /* i */
    { 340, 1350, 2350, 70, 100, 140 },  /* u (非円唇) */
    { 480, 1950, 2600, 70, 100, 150 },  /* e */
    { 480,  850, 2550, 80,  90, 140 },  /* o */
};

/* 60 dB を基準 1.0 とした線形振幅 */
static float db(float x)
{
    return x <= 0.0f ? 0.0f : powf(10.0f, (x - 60.0f) / 20.0f);
}

static void vowel_formants(CtFrame* f, int v, const CtVoice* vc)
{
    const float* t = VF[v >= 0 && v <= V_O ? v : 0];
    f->f[0] = t[0]; f->f[1] = t[1]; f->f[2] = t[2]; f->f[3] = 3500; f->f[4] = 4500;
    f->b[0] = t[3] + vc->b1add; f->b[1] = t[4]; f->b[2] = t[5]; f->b[3] = 250; f->b[4] = 300;
}

/* 子音の調音位置と後続母音からフォルマント（ローカス）を決める */
static void locus_formants(CtFrame* f, int place, int v, const CtVoice* vc)
{
    float F2v, F3v, l;
    vowel_formants(f, v, vc);
    F2v = f->f[1];
    F3v = f->f[2];
    switch (place) {
    case PL_LAB:
        f->f[1] = 800 + 0.45f * (F2v - 800);
        f->f[2] = 2200 + 0.5f * (F3v - 2200);
        break;
    case PL_ALV:
        f->f[1] = 1700 + 0.45f * (F2v - 1700);
        f->f[2] = 2650 + 0.4f * (F3v - 2650);
        break;
    case PL_PAL:
        f->f[1] = 2150 + 0.3f * (F2v - 2150);
        f->f[2] = 2900 + 0.3f * (F3v - 2900);
        break;
    case PL_VEL:
        l = (v == V_I || v == V_E) ? 2300.0f : F2v + 250.0f;
        f->f[1] = l;
        f->f[2] = 0.5f * (l + 500.0f) + 0.5f * F3v;
        break;
    default:
        return;
    }
    if (f->f[2] < f->f[1] + 250) f->f[2] = f->f[1] + 250;
    f->f[0] = 260;
}

static void fric_res(CtFrame* f, int k, float freq, float bw, float amp_db)
{
    f->fr_f[k] = freq;
    f->fr_b[k] = bw;
    f->fr_a[k] = db(amp_db);
}

/* 摩擦音のノイズ形状。gain は全体の dB 補正 */
static void fric_shape(CtFrame* f, int cons, float gain)
{
    switch (cons) {
    case C_S: case C_Z: case C_TS:
        fric_res(f, 0, 4800, 700, 54 + gain);
        fric_res(f, 1, 6800, 1200, 50 + gain);
        fric_res(f, 2, 3600, 500, 36 + gain);
        break;
    case C_SH: case C_J: case C_CH:
        fric_res(f, 0, 2900, 450, 53 + gain);
        fric_res(f, 1, 4300, 900, 52 + gain);
        fric_res(f, 2, 6000, 1500, 43 + gain);
        break;
    case C_HY:
        fric_res(f, 0, 3400, 500, 52 + gain);
        fric_res(f, 1, 4600, 900, 44 + gain);
        f->ah = db(44 + gain);
        break;
    case C_F:
        fric_res(f, 0, 1400, 1200, 40 + gain);
        fric_res(f, 1, 4000, 2500, 38 + gain);
        f->ab = db(32 + gain);
        f->ah = db(46 + gain);
        break;
    case C_H:
        f->ah = db(56 + gain);
        f->b[0] *= 2.0f;
        break;
    default:
        break;
    }
}

static void burst_shape(CtFrame* f, int cons)
{
    float g = ct_cons_info(cons)->voiced ? -8.0f : 0.0f;
    switch (ct_cons_info(cons)->place) {
    case PL_LAB:
        fric_res(f, 0, 900, 1500, 50 + g);
        f->ab = db(44 + g);
        break;
    case PL_ALV:
        fric_res(f, 0, 4200, 1500, 58 + g);
        fric_res(f, 1, 5800, 1500, 54 + g);
        break;
    case PL_VEL:
        fric_res(f, 0, f->f[1], 300, 58 + g);
        fric_res(f, 1, f->f[2], 500, 50 + g);
        break;
    default:
        break;
    }
}

static void nasal_shape(CtFrame* f, int place)
{
    f->av = db(52);
    f->fnp = 270; f->bnp = 100;
    f->fnz = 450; f->bnz = 100;
    f->f[0] = 480; f->b[0] = 100;
    f->b[1] = 300; f->b[2] = 300;
    switch (place) {
    case NP_M:      f->f[1] = 1100; f->f[2] = 2150; break;
    case NP_N:      f->f[1] = 1500; f->f[2] = 2500; break;
    case NP_NG:     f->f[1] = 1900; f->f[2] = 2600; break;
    default:        f->f[1] = 1250; f->f[2] = 2500; f->f[0] = 420; break; /* 語末の「ん」 */
    }
}

void ct_seg_target(const CtSeg* s, const CtVoice* vc, float fs, CtFrame* f)
{
    const CtConsInfo* ci = ct_cons_info(s->cons);
    int v = s->vowel ? s->vowel : V_A;
    float fric_scale = sqrtf(vc->fscale);
    int k;

    memset(f, 0, sizeof *f);
    vowel_formants(f, v, vc);
    f->fnp = 270; f->bnp = 100; f->fnz = 270; f->bnz = 100;   /* 鼻腔なし（極零が相殺） */
    f->fr_f[0] = 3000; f->fr_f[1] = 4500; f->fr_f[2] = 6000;
    f->fr_b[0] = 500;  f->fr_b[1] = 800;  f->fr_b[2] = 1000;

    switch (s->kind) {
    case S_SIL:
        break;
    case S_VOWEL:
        f->av = db(v == V_I || v == V_U ? 58.0f : 60.0f);
        f->ah = db(vc->breath);
        break;
    case S_DEVVOWEL:
        f->ah = db(48);
        f->b[0] *= 2.5f;
        if (s->cons == C_S || s->cons == C_SH || s->cons == C_TS || s->cons == C_CH || s->cons == C_HY)
            fric_shape(f, s->cons, -10);
        break;
    case S_CLOSURE:
        locus_formants(f, ci->place, v, vc);
        break;
    case S_VBAR:
        locus_formants(f, ci->place, v, vc);
        f->av = db(42);
        f->f[0] = 200;
        f->b[0] = 80;
        break;
    case S_BURST:
        locus_formants(f, ci->place, v, vc);
        burst_shape(f, s->cons);
        break;
    case S_ASP:
        locus_formants(f, ci->place, v, vc);
        f->ah = db(54);
        f->b[0] *= 2.0f;
        if (ci->place == PL_VEL) fric_res(f, 0, f->f[1], 300, 44);
        break;
    case S_FRIC:
        if (ci->place != PL_GLOT) locus_formants(f, ci->place, v, vc);
        fric_shape(f, s->cons, s->aux ? -8.0f : 0.0f);
        if (s->aux) {               /* 有声摩擦 */
            f->av = db(46);
            f->f[0] = 250;
        }
        break;
    case S_NASAL:
        nasal_shape(f, s->aux);
        break;
    case S_FLAP:
        locus_formants(f, PL_ALV, v, vc);
        f->av = db(50);
        f->f[0] = 300;
        f->b[0] = 120;
        f->b[1] = 150;
        break;
    case S_GLIDE:
        if (s->aux == G_Y) { f->f[0] = 260; f->f[1] = 2200; f->f[2] = 3000; }
        else               { f->f[0] = 320; f->f[1] = 780;  f->f[2] = 2300; }
        f->av = db(56);
        break;
    default:
        break;
    }

    /* 声道長に応じてスケーリング */
    for (k = 0; k < 5; k++) f->f[k] *= vc->fscale;
    f->fnp *= vc->fscale;
    f->fnz *= vc->fscale;
    for (k = 0; k < 3; k++) {
        f->fr_f[k] *= fric_scale;
        if (f->fr_f[k] > 0.44f * fs) f->fr_a[k] = 0.0f;
    }
}
