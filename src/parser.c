/* 記号列パーサ  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "ct_internal.h"

#define CP_LONG 0x30FC  /* ー */
#define CP_Q    0x3063  /* っ */
#define CP_N    0x3093  /* ん */

static uint32_t normalize(uint32_t c)
{
    if (c >= 0x30A1 && c <= 0x30F6) return c - 0x60;   /* カタカナ → ひらがな */
    switch (c) {
    case 0x2019: case 0xFF07:                 return '\'';
    case 0xFF0F:                              return '/';
    case 0x3001: case 0xFF0C:                 return ',';
    case 0x3002: case 0xFF0E:                 return '.';
    case 0xFF01:                              return '!';
    case 0xFF1F:                              return '?';
    case 0xFF3F:                              return '_';
    case 0x301C: case 0xFF5E: case 0x2015:    return CP_LONG;
    default:                                  return c;
    }
}

static int is_space(uint32_t c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == 0x3000;
}

static int push_mora(CtUtterance* u, const CtMora* m)
{
    if (u->n_mora == u->cap_mora) {
        int cap = u->cap_mora ? u->cap_mora * 2 : 64;
        CtMora* p = (CtMora*)realloc(u->mora, (size_t)cap * sizeof *p);
        if (!p) return CT_ERR_MEMORY;
        u->mora = p;
        u->cap_mora = cap;
    }
    u->mora[u->n_mora++] = *m;
    return CT_OK;
}

static int push_phrase(CtUtterance* u, int first, int count, int accent, int end)
{
    CtPhrase* ph;
    if (u->n_phrase == u->cap_phrase) {
        int cap = u->cap_phrase ? u->cap_phrase * 2 : 16;
        CtPhrase* p = (CtPhrase*)realloc(u->phrase, (size_t)cap * sizeof *p);
        if (!p) return CT_ERR_MEMORY;
        u->phrase = p;
        u->cap_phrase = cap;
    }
    ph = &u->phrase[u->n_phrase++];
    ph->first = first;
    ph->count = count;
    ph->accent = accent;
    ph->end = end;
    return CT_OK;
}

static int voiceless(int c)
{
    return c == C_K || c == C_S || c == C_SH || c == C_T || c == C_CH || c == C_TS ||
           c == C_H || c == C_HY || c == C_F || c == C_P;
}

/* 無声子音に挟まれた i/u、文末の「す」「つ」を無声化する */
static void auto_devoice(CtUtterance* u)
{
    int i;
    for (i = 0; i < u->n_mora; i++) {
        CtMora* m = &u->mora[i];
        const CtPhrase* ph = &u->phrase[m->phrase];
        int idx = i - ph->first;
        int last = idx == ph->count - 1;
        if (m->type != M_NORMAL || m->devoice) continue;
        if (m->vowel != V_I && m->vowel != V_U) continue;
        if (!voiceless(m->cons) || m->glide != G_NONE) continue;
        if (i > 0 && u->mora[i - 1].devoice) continue;   /* 連続無声化はしない */
        if (ph->accent == idx + 1) continue;             /* アクセント核は残す */
        if (!last || ph->end == PE_NONE) {
            if (i + 1 < u->n_mora) {
                const CtMora* nx = &u->mora[i + 1];
                if ((nx->type == M_NORMAL && voiceless(nx->cons)) || nx->type == M_Q)
                    m->devoice = 1;
            }
        } else if (ph->end == PE_PERIOD && m->vowel == V_U &&
                   (m->cons == C_S || m->cons == C_TS)) {
            m->devoice = 1;
        }
    }
}

void ct_utterance_free(CtUtterance* u)
{
    free(u->mora);
    free(u->phrase);
    memset(u, 0, sizeof *u);
}

int ct_parse(const char* text, CtUtterance* u, int* err_pos)
{
    uint32_t* cp = NULL;
    int n = 0, i = 0, err;
    int ph_first = 0, ph_accent = 0;
    int dev_pending = 0, dev_pos = -1;

    memset(u, 0, sizeof *u);
    *err_pos = -1;
    err = ct_utf8_decode(text, &cp, &n);
    if (err) { *err_pos = err == CT_ERR_UTF8 ? n : -1; return err; }
    for (i = 0; i < n; i++) cp[i] = normalize(cp[i]);

    i = 0;
    while (i < n) {
        uint32_t c = cp[i];
        int cur = u->n_mora - ph_first;
        CtMora m;
        int used = 1;

        if (is_space(c)) { i++; continue; }

        if (c == '\'') {
            if (cur == 0 || ph_accent) { err = CT_ERR_ACCENT; *err_pos = i; goto fail; }
            ph_accent = cur;
            i++;
            continue;
        }
        if (c == '/' || c == ',' || c == '.' || c == '!' || c == '?') {
            int end = c == '/' ? PE_NONE : c == ',' ? PE_COMMA : c == '?' ? PE_QUESTION : PE_PERIOD;
            if (dev_pending) { err = CT_ERR_DEVOICE; *err_pos = dev_pos; goto fail; }
            if (cur > 0) {
                if ((err = push_phrase(u, ph_first, cur, ph_accent, end)) != CT_OK) goto fail;
                ph_first = u->n_mora;
                ph_accent = 0;
            } else if (u->n_phrase > 0 && end > u->phrase[u->n_phrase - 1].end) {
                u->phrase[u->n_phrase - 1].end = end;   /* 「、。」「。？」など */
            }
            i++;
            continue;
        }
        if (c == '_') {
            if (dev_pending) { err = CT_ERR_DEVOICE; *err_pos = i; goto fail; }
            dev_pending = 1;
            dev_pos = i;
            i++;
            continue;
        }

        memset(&m, 0, sizeof m);
        m.src_pos = i;
        m.phrase = u->n_phrase;
        if (c == CP_LONG) {
            const CtMora* pv;
            if (cur == 0) { err = CT_ERR_LONG; *err_pos = i; goto fail; }
            pv = &u->mora[u->n_mora - 1];
            if (pv->type == M_Q) { err = CT_ERR_LONG; *err_pos = i; goto fail; }
            m.type = pv->type == M_N ? M_N : M_LONG;
            m.vowel = pv->vowel;
        } else if (c == CP_Q) {
            m.type = M_Q;
        } else if (c == CP_N) {
            m.type = M_N;
        } else {
            used = ct_kana_lookup(cp + i, n - i, &m.cons, &m.glide, &m.vowel);
            if (!used) { err = CT_ERR_CHAR; *err_pos = i; goto fail; }
            m.type = M_NORMAL;
        }
        if (dev_pending) {
            if (m.type != M_NORMAL) { err = CT_ERR_DEVOICE; *err_pos = dev_pos; goto fail; }
            m.devoice = 1;
            m.forced = 1;
            dev_pending = 0;
        }
        if ((err = push_mora(u, &m)) != CT_OK) goto fail;
        i += used;
    }

    if (dev_pending) { err = CT_ERR_DEVOICE; *err_pos = dev_pos; goto fail; }
    if (u->n_mora - ph_first > 0) {
        if ((err = push_phrase(u, ph_first, u->n_mora - ph_first, ph_accent, PE_PERIOD)) != CT_OK)
            goto fail;
    } else if (u->n_phrase > 0 && u->phrase[u->n_phrase - 1].end == PE_NONE) {
        u->phrase[u->n_phrase - 1].end = PE_PERIOD;
    }
    if (u->n_mora == 0) { err = CT_ERR_EMPTY; *err_pos = 0; goto fail; }

    auto_devoice(u);
    free(cp);
    return CT_OK;

fail:
    free(cp);
    ct_utterance_free(u);
    return err;
}
