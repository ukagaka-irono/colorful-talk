/* 韻律: モーラ列 → 音素セグメント列（長さ）＋ F0 目標点  SPDX-License-Identifier: MIT */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ct_internal.h"

#define MORA_MS   125.0f   /* 標準話速での 1 モーラの目安 */
#define LEAD_MS    60.0f
#define TRAIL_MS  150.0f

typedef struct { float t0, t1; int has; } MoraTime;

static int add_seg(CtPlan* p, int kind, int cons, int vowel, int aux, float dur)
{
    CtSeg* s;
    if (p->n_seg == p->cap_seg) {
        int cap = p->cap_seg ? p->cap_seg * 2 : 128;
        CtSeg* q = (CtSeg*)realloc(p->seg, (size_t)cap * sizeof *q);
        if (!q) return CT_ERR_MEMORY;
        p->seg = q;
        p->cap_seg = cap;
    }
    if (dur < CT_FRAME_MS) dur = CT_FRAME_MS;
    s = &p->seg[p->n_seg++];
    s->kind = (uint8_t)kind;
    s->cons = (uint8_t)cons;
    s->vowel = (uint8_t)vowel;
    s->aux = (uint8_t)aux;
    s->dur = dur;
    p->total_ms += dur;
    return CT_OK;
}

static int add_f0(CtPlan* p, float t, float st)
{
    if (p->n_f0 == p->cap_f0) {
        int cap = p->cap_f0 ? p->cap_f0 * 2 : 128;
        CtF0Point* q = (CtF0Point*)realloc(p->f0, (size_t)cap * sizeof *q);
        if (!q) return CT_ERR_MEMORY;
        p->f0 = q;
        p->cap_f0 = cap;
    }
    if (p->n_f0 > 0 && t <= p->f0[p->n_f0 - 1].t) t = p->f0[p->n_f0 - 1].t + 0.1f;
    p->f0[p->n_f0].t = t;
    p->f0[p->n_f0].st = st;
    p->n_f0++;
    return CT_OK;
}

/* 撥音の調音位置を後続子音に合わせる */
static int nasal_place(const CtMora* nx)
{
    const CtConsInfo* ci;
    if (!nx || nx->type != M_NORMAL || nx->cons == C_NONE) return NP_UVULAR;
    ci = ct_cons_info(nx->cons);
    if (ci->manner == MN_FRIC) return NP_UVULAR;
    switch (ci->place) {
    case PL_LAB: return NP_M;
    case PL_ALV: case PL_PAL: return NP_N;
    case PL_VEL: return NP_NG;
    default: return NP_UVULAR;
    }
}

static float fric_ms(int cons)
{
    switch (cons) {
    case C_S:  return 95;
    case C_SH: return 105;
    case C_HY: return 80;
    case C_F:  return 75;
    default:   return 70;  /* h */
    }
}

/* 子音部（＋半母音）を出力し、標準話速での長さを返す */
static int emit_cons(CtPlan* p, const CtMora* m, int after_q, float sc, float* ms)
{
    const CtConsInfo* ci = ct_cons_info(m->cons);
    int cv = m->glide == G_Y ? V_I : m->glide == G_W ? V_U : m->vowel;
    float d[3] = { 0, 0, 0 };
    int err = CT_OK;

    switch (ci->manner) {
    case MN_STOP:
        if (ci->voiced) {
            d[0] = after_q ? 20.0f : 40.0f; d[1] = 5;
            err = add_seg(p, S_VBAR, m->cons, cv, 0, d[0] * sc);
            if (!err) err = add_seg(p, S_BURST, m->cons, cv, 0, d[1] * sc);
        } else {
            d[0] = after_q ? 15.0f : 50.0f; d[1] = 6;
            d[2] = ci->place == PL_LAB ? 10.0f : ci->place == PL_ALV ? 14.0f : 26.0f;
            err = add_seg(p, S_CLOSURE, m->cons, cv, 0, d[0] * sc);
            if (!err) err = add_seg(p, S_BURST, m->cons, cv, 0, d[1] * sc);
            if (!err) err = add_seg(p, S_ASP, m->cons, cv, 0, d[2] * sc);
        }
        break;
    case MN_AFFR:
        if (ci->voiced) {
            d[0] = after_q ? 15.0f : 25.0f; d[1] = 50;
            err = add_seg(p, S_VBAR, m->cons, cv, 0, d[0] * sc);
            if (!err) err = add_seg(p, S_FRIC, m->cons, cv, 1, d[1] * sc);
        } else {
            d[0] = after_q ? 15.0f : 40.0f; d[1] = m->cons == C_TS ? 60.0f : 65.0f;
            err = add_seg(p, S_CLOSURE, m->cons, cv, 0, d[0] * sc);
            if (!err) err = add_seg(p, S_FRIC, m->cons, cv, 0, d[1] * sc);
        }
        break;
    case MN_FRIC:
        d[0] = fric_ms(m->cons);
        err = add_seg(p, S_FRIC, m->cons, cv, 0, d[0] * sc);
        break;
    case MN_NASAL:
        d[0] = 55;
        err = add_seg(p, S_NASAL, m->cons, cv, m->cons == C_M ? NP_M : NP_N, d[0] * sc);
        break;
    case MN_FLAP:
        d[0] = 22;
        err = add_seg(p, S_FLAP, m->cons, cv, 0, d[0] * sc);
        break;
    default:
        break;
    }
    if (!err && m->glide) {
        d[2] += m->cons ? 35 : 50;
        err = add_seg(p, S_GLIDE, m->cons, m->vowel, m->glide, (m->cons ? 35 : 50) * sc);
    }
    *ms = d[0] + d[1] + d[2];
    return err;
}

static float pause_ms(int end)
{
    switch (end) {
    case PE_COMMA:    return 220;
    case PE_PERIOD:   return 400;
    case PE_QUESTION: return 400;
    default:          return 0;
    }
}

static int build_segments(const CtUtterance* u, float sc, CtPlan* p, MoraTime* mt)
{
    int pi, i, err;
    int prev_vowel = V_A;

    if ((err = add_seg(p, S_SIL, 0, u->mora[0].vowel, 0, LEAD_MS)) != CT_OK) return err;

    for (pi = 0; pi < u->n_phrase; pi++) {
        const CtPhrase* ph = &u->phrase[pi];
        int last_phrase = pi == u->n_phrase - 1;
        for (i = ph->first; i < ph->first + ph->count; i++) {
            const CtMora* m = &u->mora[i];
            int last_in_phrase = i == ph->first + ph->count - 1;
            const CtMora* nx = (last_in_phrase && ph->end != PE_NONE) || i + 1 >= u->n_mora
                             ? NULL : &u->mora[i + 1];
            int after_q = i > 0 && u->mora[i - 1].type == M_Q;
            float lengthen = !last_in_phrase ? 1.0f
                           : ph->end == PE_NONE ? 1.0f
                           : ph->end == PE_COMMA ? 1.15f : 1.3f;
            float cms = 0, vdur;

            mt[i].has = 0;
            switch (m->type) {
            case M_NORMAL:
                if ((err = emit_cons(p, m, after_q, sc, &cms)) != CT_OK) return err;
                mt[i].t0 = p->total_ms;
                if (m->devoice) {
                    err = add_seg(p, S_DEVVOWEL, m->cons, m->vowel, 0, 45 * sc);
                } else {
                    vdur = MORA_MS - 0.5f * cms;
                    if (vdur < 55) vdur = 55;
                    err = add_seg(p, S_VOWEL, 0, m->vowel, 0, vdur * sc * lengthen);
                }
                if (err) return err;
                mt[i].t1 = p->total_ms;
                mt[i].has = 1;
                prev_vowel = m->vowel;
                break;
            case M_N:
                mt[i].t0 = p->total_ms;
                if ((err = add_seg(p, S_NASAL, 0, prev_vowel, nasal_place(nx), 90 * sc * lengthen)) != CT_OK)
                    return err;
                mt[i].t1 = p->total_ms;
                mt[i].has = 1;
                break;
            case M_LONG:
                mt[i].t0 = p->total_ms;
                if ((err = add_seg(p, S_VOWEL, 0, m->vowel, 0, 105 * sc * lengthen)) != CT_OK) return err;
                mt[i].t1 = p->total_ms;
                mt[i].has = 1;
                break;
            case M_Q: {
                const CtConsInfo* ci = nx ? ct_cons_info(nx->cons) : ct_cons_info(C_NONE);
                int nv = nx ? nx->vowel : prev_vowel;
                if (nx && nx->type == M_NORMAL && (ci->manner == MN_STOP || ci->manner == MN_AFFR))
                    err = add_seg(p, S_CLOSURE, nx->cons, nv, 0, 85 * sc);
                else if (nx && nx->type == M_NORMAL && ci->manner == MN_FRIC)
                    err = add_seg(p, S_FRIC, nx->cons, nv, 0, 70 * sc);
                else
                    err = add_seg(p, S_SIL, 0, prev_vowel, 0, 60 * sc);
                if (err) return err;
                break;
            }
            default:
                break;
            }
        }
        if (last_phrase) {
            float pm = pause_ms(ph->end) * sc;
            err = add_seg(p, S_SIL, 0, prev_vowel, 0, pm > TRAIL_MS ? pm : TRAIL_MS);
        } else if (ph->end != PE_NONE) {
            err = add_seg(p, S_SIL, 0, u->mora[ph[1].first].vowel, 0, pause_ms(ph->end) * sc);
        } else {
            err = CT_OK;
        }
        if (err) return err;
    }
    return CT_OK;
}

/* 東京方言のアクセント規則による F0 目標点 */
static int build_f0(const CtUtterance* u, const CtVoice* v, float accent, CtPlan* p, const MoraTime* mt)
{
    float r = accent / 100.0f * v->f0range;
    float decl = 1.6f * (r < 1.0f ? r : 1.0f);   /* 自然下降 [半音/秒] */
    float down = 0.0f, sent_t0 = 0.0f;
    int sent_start = 1, pi, k, err;

    for (pi = 0; pi < u->n_phrase; pi++) {
        const CtPhrase* ph = &u->phrase[pi];
        int acc = ph->accent;
        for (k = 1; k <= ph->count; k++) {
            const MoraTime* t = &mt[ph->first + k - 1];
            float mid, st;
            int high;
            if (!t->has) continue;
            mid = 0.5f * (t->t0 + t->t1);
            if (sent_start) { sent_t0 = mid; down = 0.0f; sent_start = 0; }

            if (acc == 1) high = k == 1;
            else          high = k >= 2 && (acc == 0 || k <= acc);
            st = high ? 4.0f * r : (k == 1 ? 0.0f : -1.5f * r);
            st -= down + decl * (mid - sent_t0) / 1000.0f;

            if (k == ph->count && ph->end == PE_PERIOD) {
                err = add_f0(p, mid, st);
                if (!err) err = add_f0(p, t->t1, st - 2.0f * r);
            } else if (k == ph->count && ph->end == PE_QUESTION) {
                err = add_f0(p, t->t0 + 0.3f * (t->t1 - t->t0), st);
                if (!err) err = add_f0(p, t->t1, st + 6.0f * (r > 0.5f ? r : 0.5f));
            } else {
                err = add_f0(p, mid, st);
                if (!err && k == acc) err = add_f0(p, t->t1, st);  /* 核の直後で下げる */
            }
            if (err) return err;
        }
        if (acc > 0) down += 1.5f * r;
        if (ph->end == PE_COMMA) down *= 0.5f;
        if (ph->end == PE_PERIOD || ph->end == PE_QUESTION) sent_start = 1;
    }
    return CT_OK;
}

int ct_build_plan(const CtUtterance* u, const CtVoice* v, float speed, float accent, CtPlan* p)
{
    float sc = 100.0f / speed;
    MoraTime* mt;
    int err;

    memset(p, 0, sizeof *p);
    if (u->n_mora == 0) return CT_ERR_EMPTY;
    mt = (MoraTime*)calloc((size_t)u->n_mora, sizeof *mt);
    if (!mt) return CT_ERR_MEMORY;
    err = build_segments(u, sc, p, mt);
    if (!err) err = build_f0(u, v, accent, p, mt);
    free(mt);
    if (err) ct_plan_free(p);
    return err;
}

void ct_plan_free(CtPlan* p)
{
    free(p->seg);
    free(p->f0);
    memset(p, 0, sizeof *p);
}
