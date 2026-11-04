/*
 * カスケード／パラレル型フォルマント合成器
 * Klatt (1980) "Software for a cascade/parallel formant synthesizer" の方式を独自に実装したもの。
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdlib.h>
#include "ct_internal.h"

/* 内部ゲイン（経験的に調整） */
#define K_ASP  0.30   /* 気息音 */
#define K_FRIC 0.20   /* 摩擦音（パラレル枝） */

typedef struct { double a, b, c, y1, y2; } Res;
typedef struct { double a, b, c, x1, x2; } Anti;

/* カスケード用: 直流ゲイン 1 */
static void res_cascade(Res* r, double f, double bw, double fs)
{
    double T = 1.0 / fs, e;
    if (f <= 0.0 || f >= 0.46 * fs) { r->a = 1.0; r->b = 0.0; r->c = 0.0; return; }
    e = exp(-CT_PI * bw * T);
    r->c = -e * e;
    r->b = 2.0 * e * cos(2.0 * CT_PI * f * T);
    r->a = 1.0 - r->b - r->c;
}

/* パラレル用: 共振周波数でのゲインを 1 に正規化 */
static void res_parallel(Res* r, double f, double bw, double fs)
{
    double T = 1.0 / fs, e, w, re, im;
    if (f <= 0.0 || f >= 0.46 * fs) { r->a = 0.0; r->b = 0.0; r->c = 0.0; return; }
    e = exp(-CT_PI * bw * T);
    r->c = -e * e;
    r->b = 2.0 * e * cos(2.0 * CT_PI * f * T);
    w = 2.0 * CT_PI * f * T;
    re = 1.0 - r->b * cos(w) - r->c * cos(2.0 * w);
    im = r->b * sin(w) + r->c * sin(2.0 * w);
    r->a = sqrt(re * re + im * im);
}

static double res_run(Res* r, double x)
{
    double y = r->a * x + r->b * r->y1 + r->c * r->y2;
    r->y2 = r->y1;
    r->y1 = y;
    return y;
}

static void anti_set(Anti* z, double f, double bw, double fs)
{
    double T = 1.0 / fs, e, a, b, c;
    if (f <= 0.0 || f >= 0.46 * fs) { z->a = 1.0; z->b = 0.0; z->c = 0.0; return; }
    e = exp(-CT_PI * bw * T);
    c = -e * e;
    b = 2.0 * e * cos(2.0 * CT_PI * f * T);
    a = 1.0 - b - c;
    z->a = 1.0 / a;
    z->b = -b / a;
    z->c = -c / a;
}

static double anti_run(Anti* z, double x)
{
    double y = z->a * x + z->b * z->x1 + z->c * z->x2;
    z->x2 = z->x1;
    z->x1 = x;
    return y;
}

/* 決定的な乱数（xorshift32） */
static double uni(uint32_t* s)
{
    uint32_t x = *s;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *s = x;
    return (x >> 8) * (1.0 / 16777216.0);
}
static double gauss(uint32_t* s)
{
    return (uni(s) + uni(s) + uni(s) + uni(s) - 2.0) * 1.2247;
}

int ct_klatt_render(const CtFrame* fr, int nfr, const CtVoice* v, int fs, float** out, int* n)
{
    const double spf = fs * (double)CT_FRAME_MS / 1000.0;
    const int N = (int)(nfr * spf);
    const double rad_gain = fs / 16000.0;
    const double tilt_a = v->tilt <= 0 ? 0.0 : (v->tilt >= 25 ? 0.85 : v->tilt / 25.0 * 0.85);
    float* o;
    Res casc[5] = {{0}}, np = {0}, par[3] = {{0}};
    Anti nz = {0};
    uint32_t rng = 0x9E3779B9u;
    double phase = 0.0, T0 = 0.0, Nopen = 0.0;
    double tilt_y = 0.0, asp_lp = 0.0, prev = 0.0;
    int cur = -1, i, k;

    *out = NULL;
    *n = 0;
    if (nfr <= 0) return CT_ERR_EMPTY;
    o = (float*)malloc((size_t)(N > 0 ? N : 1) * sizeof *o);
    if (!o) return CT_ERR_MEMORY;

    for (i = 0; i < N; i++) {
        double pos = i / spf, fra, av, ah, ab, fa[3], g, src, x, fn, p, y, mod;
        int fi = (int)pos;
        const CtFrame *A, *B;
        if (fi >= nfr) fi = nfr - 1;
        fra = pos - fi;
        A = &fr[fi];
        B = &fr[fi + 1 < nfr ? fi + 1 : fi];

        if (fi != cur) {
            cur = fi;
            for (k = 0; k < 5; k++) res_cascade(&casc[k], A->f[k], A->b[k], fs);
            res_cascade(&np, A->fnp, A->bnp, fs);
            anti_set(&nz, A->fnz, A->bnz, fs);
            for (k = 0; k < 3; k++) res_parallel(&par[k], A->fr_f[k], A->fr_b[k], fs);
        }

        av = A->av + (B->av - A->av) * fra;
        ah = A->ah + (B->ah - A->ah) * fra;
        ab = A->ab + (B->ab - A->ab) * fra;
        for (k = 0; k < 3; k++) fa[k] = A->fr_a[k] + (B->fr_a[k] - A->fr_a[k]) * fra;

        /* 声帯音源: 周期の頭で次の周期長を決める */
        if (phase >= T0) {
            double f0 = A->f0 + (B->f0 - A->f0) * fra;
            if (f0 < 40.0) f0 = 40.0;
            phase -= T0;
            if (phase < 0.0 || phase > 1.0) phase = 0.0;
            T0 = fs / f0 * (1.0 + v->jitter * (uni(&rng) * 2.0 - 1.0));
            Nopen = v->oq * T0;
        }
        if (phase < Nopen) {
            double t = phase / Nopen;
            g = 6.75 * (t * t - t * t * t);   /* 声門体積流 (ピーク 1) */
        } else {
            g = 0.0;
        }
        mod = (av > 1e-4 && phase >= Nopen) ? 0.5 : 1.0;  /* 閉鎖期は雑音を弱める */
        phase += 1.0;

        tilt_y = (1.0 - tilt_a) * g + tilt_a * tilt_y;
        asp_lp = 0.5 * asp_lp + 0.5 * gauss(&rng);
        src = av * tilt_y + ah * asp_lp * mod * K_ASP;

        /* カスケード枝: 鼻腔極 → 鼻腔零 → F5 … F1 */
        x = res_run(&np, src);
        x = anti_run(&nz, x);
        for (k = 4; k >= 0; k--) x = res_run(&casc[k], x);

        /* パラレル枝（摩擦） */
        fn = gauss(&rng) * mod;
        p = fa[0] * res_run(&par[0], fn) - fa[1] * res_run(&par[1], fn)
          + fa[2] * res_run(&par[2], fn) + ab * fn;

        y = x + p * K_FRIC;
        o[i] = (float)((y - prev) * rad_gain);   /* 放射特性 */
        prev = y;
    }
    *out = o;
    *n = N;
    return CT_OK;
}
