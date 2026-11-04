/* colorful-talk 内部ヘッダ  SPDX-License-Identifier: MIT */
#ifndef CT_INTERNAL_H
#define CT_INTERNAL_H

#include <stddef.h>
#include <stdint.h>
#include "colorful_talk.h"

#define CT_PI       3.14159265358979323846
#define CT_FRAME_MS 2.0f   /* パラメータ軌跡のフレーム長 */

/* ---- 音素 ---------------------------------------------------------- */
enum { C_NONE = 0, C_K, C_G, C_S, C_SH, C_Z, C_J, C_T, C_D, C_TS, C_CH,
       C_N, C_H, C_HY, C_F, C_B, C_P, C_M, C_R, C_COUNT };
enum { G_NONE = 0, G_Y, G_W };
enum { V_NONE = 0, V_A, V_I, V_U, V_E, V_O };

/* 調音様式・調音位置 */
enum { MN_NONE = 0, MN_STOP, MN_FRIC, MN_AFFR, MN_NASAL, MN_FLAP };
enum { PL_NONE = 0, PL_LAB, PL_ALV, PL_PAL, PL_VEL, PL_GLOT };
typedef struct { uint8_t manner, place, voiced; } CtConsInfo;
const CtConsInfo* ct_cons_info(int cons);

/* ---- 記号列の解析結果 ------------------------------------------------ */
enum { M_NORMAL = 0, M_N, M_Q, M_LONG };       /* 通常 / 撥音 / 促音 / 長音 */
enum { PE_NONE = 0, PE_COMMA, PE_PERIOD, PE_QUESTION };

typedef struct {
    uint8_t type, cons, glide, vowel;
    uint8_t devoice;   /* 1 = 母音無声化 */
    uint8_t forced;    /* '_' で明示された */
    int     phrase;    /* 所属アクセント句 */
    int     src_pos;   /* 記号列中の文字位置 */
} CtMora;

typedef struct {
    int first, count;  /* モーラ範囲 */
    int accent;        /* 0 = 平板, k = k モーラ目の後で下がる */
    int end;           /* PE_* */
} CtPhrase;

typedef struct {
    CtMora*   mora;   int n_mora,   cap_mora;
    CtPhrase* phrase; int n_phrase, cap_phrase;
} CtUtterance;

int  ct_utf8_decode(const char* s, uint32_t** out, int* n); /* 失敗時 *n = 不正位置 */
int  ct_kana_lookup(const uint32_t* cp, int n, uint8_t* cons, uint8_t* glide, uint8_t* vowel);
int  ct_parse(const char* text, CtUtterance* u, int* err_pos);
void ct_utterance_free(CtUtterance* u);

/* ---- 声 ------------------------------------------------------------- */
typedef struct {
    float f0;       /* 基準 F0 [Hz] */
    float fscale;   /* フォルマント倍率（声道長） */
    float oq;       /* 声門開放率 */
    float tilt;     /* 声帯音源の高域減衰 [dB] */
    float breath;   /* 有声時の気息音 [dB] */
    float jitter;   /* 周期ゆらぎ */
    float f0range;  /* 抑揚幅の倍率 */
    float b1add;    /* B1 の追加幅 [Hz] */
} CtVoice;

/* ---- 音素セグメント列（韻律付き） ------------------------------------- */
enum { S_SIL = 0, S_VOWEL, S_DEVVOWEL, S_CLOSURE, S_VBAR, S_BURST, S_ASP,
       S_FRIC, S_NASAL, S_FLAP, S_GLIDE };
enum { NP_M = 0, NP_N, NP_NG, NP_UVULAR };

typedef struct {
    uint8_t kind;
    uint8_t cons;   /* 子音 ID（子音系セグメント, 無声化母音では直前子音） */
    uint8_t vowel;  /* 自分の母音 / 文脈となる母音 */
    uint8_t aux;    /* 鼻音の位置, 半母音の種類, 摩擦の有声フラグ */
    float   dur;    /* ms */
} CtSeg;

typedef struct { float t; float st; } CtF0Point; /* 時刻 ms, 基準からの半音 */

typedef struct {
    CtSeg*     seg; int n_seg, cap_seg;
    CtF0Point* f0;  int n_f0,  cap_f0;
    float total_ms;
} CtPlan;

int  ct_build_plan(const CtUtterance* u, const CtVoice* v, float speed, float accent, CtPlan* p);
void ct_plan_free(CtPlan* p);

/* ---- フレーム（合成器パラメータ） ------------------------------------- */
typedef struct {
    float f0;
    float av, ah, ab;              /* 有声 / 気息 / バイパス 振幅（線形） */
    float f[5], b[5];              /* カスケード F1–F5 */
    float fnp, bnp, fnz, bnz;      /* 鼻腔 極・零 */
    float fr_f[3], fr_b[3], fr_a[3]; /* 摩擦用パラレル共振器 */
} CtFrame;

void ct_seg_target(const CtSeg* s, const CtVoice* v, float fs, CtFrame* out);
int  ct_build_frames(const CtPlan* p, const CtVoice* v, float fs, CtFrame** frames, int* n);
int  ct_klatt_render(const CtFrame* fr, int nfr, const CtVoice* v, int fs, float** out, int* n);

unsigned char* ct_wav_build(const short* pcm, int n, int fs, int* size);

#endif
