/* セグメント列 → 2ms フレームのパラメータ軌跡（補間・平滑化）  SPDX-License-Identifier: MIT */
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include "ct_internal.h"

/* 前向き＋後ろ向きの 1 次平滑化（位相ずれなし） */
static void smooth(CtFrame* fr, int n, size_t off, float tau_ms)
{
    float a = expf(-CT_FRAME_MS / tau_ms), y;
    int i;
#define FIELD(i) (*(float*)((char*)&fr[i] + off))
    y = FIELD(0);
    for (i = 0; i < n; i++) { y = a * y + (1 - a) * FIELD(i); FIELD(i) = y; }
    y = FIELD(n - 1);
    for (i = n - 1; i >= 0; i--) { y = a * y + (1 - a) * FIELD(i); FIELD(i) = y; }
#undef FIELD
}

int ct_build_frames(const CtPlan* p, const CtVoice* v, float fs, CtFrame** out, int* nout)
{
    int n = (int)ceilf(p->total_ms / CT_FRAME_MS) + 1;
    CtFrame* fr;
    int i, k, si = 0, pi = 0;
    float seg_start = 0.0f;

    *out = NULL;
    *nout = 0;
    if (p->n_seg == 0) return CT_ERR_EMPTY;
    fr = (CtFrame*)calloc((size_t)n, sizeof *fr);
    if (!fr) return CT_ERR_MEMORY;

    /* 各フレームの中心時刻が属するセグメントの目標値 */
    for (i = 0; i < n; i++) {
        float t = (i + 0.5f) * CT_FRAME_MS;
        while (si < p->n_seg - 1 && t >= seg_start + p->seg[si].dur) {
            seg_start += p->seg[si].dur;
            si++;
        }
        ct_seg_target(&p->seg[si], v, fs, &fr[i]);
    }

    /* F0: 目標点を半音領域で直線補間 */
    for (i = 0; i < n; i++) {
        float t = i * CT_FRAME_MS, st;
        if (p->n_f0 == 0) {
            st = 0.0f;
        } else if (t <= p->f0[0].t) {
            st = p->f0[0].st;
        } else {
            while (pi < p->n_f0 - 1 && t > p->f0[pi + 1].t) pi++;
            if (pi >= p->n_f0 - 1) {
                st = p->f0[p->n_f0 - 1].st;
            } else {
                const CtF0Point* a = &p->f0[pi];
                const CtF0Point* b = &p->f0[pi + 1];
                float d = b->t - a->t;
                st = d > 0 ? a->st + (b->st - a->st) * (t - a->t) / d : b->st;
            }
        }
        fr[i].f0 = st;
    }

    /* 平滑化: フォルマントは調音結合、振幅はクリック防止 */
    smooth(fr, n, offsetof(CtFrame, f0), 15.0f);
    for (k = 0; k < 5; k++) {
        smooth(fr, n, offsetof(CtFrame, f) + k * sizeof(float), 9.0f);
        smooth(fr, n, offsetof(CtFrame, b) + k * sizeof(float), 6.0f);
    }
    smooth(fr, n, offsetof(CtFrame, fnz), 8.0f);
    smooth(fr, n, offsetof(CtFrame, bnz), 8.0f);
    smooth(fr, n, offsetof(CtFrame, av), 3.0f);
    smooth(fr, n, offsetof(CtFrame, ah), 3.0f);
    smooth(fr, n, offsetof(CtFrame, ab), 1.5f);
    for (k = 0; k < 3; k++) {
        smooth(fr, n, offsetof(CtFrame, fr_f) + k * sizeof(float), 2.0f);
        smooth(fr, n, offsetof(CtFrame, fr_a) + k * sizeof(float), 1.5f);
    }

    for (i = 0; i < n; i++) fr[i].f0 = v->f0 * powf(2.0f, fr[i].f0 / 12.0f);

    *out = fr;
    *nout = n;
    return CT_OK;
}
