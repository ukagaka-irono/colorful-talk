/* ひらがな → 音素（子音・半母音・母音）表  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include "ct_internal.h"

typedef struct { const char* k; uint8_t c, g, v; } KanaEnt;

#define A V_A
#define I V_I
#define U V_U
#define E V_E
#define O V_O

static const KanaEnt TABLE[] = {
    /* 母音 */
    {"あ",0,0,A},{"い",0,0,I},{"う",0,0,U},{"え",0,0,E},{"お",0,0,O},
    {"ぁ",0,0,A},{"ぃ",0,0,I},{"ぅ",0,0,U},{"ぇ",0,0,E},{"ぉ",0,0,O},
    {"を",0,0,O},{"ゐ",0,0,I},{"ゑ",0,0,E},
    /* か行 */
    {"か",C_K,0,A},{"き",C_K,0,I},{"く",C_K,0,U},{"け",C_K,0,E},{"こ",C_K,0,O},
    {"ゕ",C_K,0,A},{"ゖ",C_K,0,E},
    {"が",C_G,0,A},{"ぎ",C_G,0,I},{"ぐ",C_G,0,U},{"げ",C_G,0,E},{"ご",C_G,0,O},
    /* さ行 */
    {"さ",C_S,0,A},{"し",C_SH,0,I},{"す",C_S,0,U},{"せ",C_S,0,E},{"そ",C_S,0,O},
    {"ざ",C_Z,0,A},{"じ",C_J,0,I},{"ず",C_Z,0,U},{"ぜ",C_Z,0,E},{"ぞ",C_Z,0,O},
    /* た行 */
    {"た",C_T,0,A},{"ち",C_CH,0,I},{"つ",C_TS,0,U},{"て",C_T,0,E},{"と",C_T,0,O},
    {"だ",C_D,0,A},{"ぢ",C_J,0,I},{"づ",C_Z,0,U},{"で",C_D,0,E},{"ど",C_D,0,O},
    /* な行 */
    {"な",C_N,0,A},{"に",C_N,0,I},{"ぬ",C_N,0,U},{"ね",C_N,0,E},{"の",C_N,0,O},
    /* は行 */
    {"は",C_H,0,A},{"ひ",C_HY,0,I},{"ふ",C_F,0,U},{"へ",C_H,0,E},{"ほ",C_H,0,O},
    {"ば",C_B,0,A},{"び",C_B,0,I},{"ぶ",C_B,0,U},{"べ",C_B,0,E},{"ぼ",C_B,0,O},
    {"ぱ",C_P,0,A},{"ぴ",C_P,0,I},{"ぷ",C_P,0,U},{"ぺ",C_P,0,E},{"ぽ",C_P,0,O},
    /* ま行 */
    {"ま",C_M,0,A},{"み",C_M,0,I},{"む",C_M,0,U},{"め",C_M,0,E},{"も",C_M,0,O},
    /* や行・わ行 */
    {"や",0,G_Y,A},{"ゆ",0,G_Y,U},{"よ",0,G_Y,O},
    {"ゃ",0,G_Y,A},{"ゅ",0,G_Y,U},{"ょ",0,G_Y,O},
    {"わ",0,G_W,A},{"ゎ",0,G_W,A},
    /* ら行 */
    {"ら",C_R,0,A},{"り",C_R,0,I},{"る",C_R,0,U},{"れ",C_R,0,E},{"ろ",C_R,0,O},
    /* ゔ */
    {"ゔ",C_B,0,U},

    /* 拗音 */
    {"きゃ",C_K,G_Y,A},{"きゅ",C_K,G_Y,U},{"きょ",C_K,G_Y,O},{"きぇ",C_K,G_Y,E},
    {"ぎゃ",C_G,G_Y,A},{"ぎゅ",C_G,G_Y,U},{"ぎょ",C_G,G_Y,O},{"ぎぇ",C_G,G_Y,E},
    {"しゃ",C_SH,0,A},{"しゅ",C_SH,0,U},{"しょ",C_SH,0,O},{"しぇ",C_SH,0,E},
    {"じゃ",C_J,0,A},{"じゅ",C_J,0,U},{"じょ",C_J,0,O},{"じぇ",C_J,0,E},
    {"ちゃ",C_CH,0,A},{"ちゅ",C_CH,0,U},{"ちょ",C_CH,0,O},{"ちぇ",C_CH,0,E},
    {"ぢゃ",C_J,0,A},{"ぢゅ",C_J,0,U},{"ぢょ",C_J,0,O},{"ぢぇ",C_J,0,E},
    {"にゃ",C_N,G_Y,A},{"にゅ",C_N,G_Y,U},{"にょ",C_N,G_Y,O},{"にぇ",C_N,G_Y,E},
    {"ひゃ",C_HY,0,A},{"ひゅ",C_HY,0,U},{"ひょ",C_HY,0,O},{"ひぇ",C_HY,0,E},
    {"びゃ",C_B,G_Y,A},{"びゅ",C_B,G_Y,U},{"びょ",C_B,G_Y,O},{"びぇ",C_B,G_Y,E},
    {"ぴゃ",C_P,G_Y,A},{"ぴゅ",C_P,G_Y,U},{"ぴょ",C_P,G_Y,O},{"ぴぇ",C_P,G_Y,E},
    {"みゃ",C_M,G_Y,A},{"みゅ",C_M,G_Y,U},{"みょ",C_M,G_Y,O},{"みぇ",C_M,G_Y,E},
    {"りゃ",C_R,G_Y,A},{"りゅ",C_R,G_Y,U},{"りょ",C_R,G_Y,O},{"りぇ",C_R,G_Y,E},

    /* 外来音 */
    {"ふぁ",C_F,0,A},{"ふぃ",C_F,0,I},{"ふぇ",C_F,0,E},{"ふぉ",C_F,0,O},{"ふゅ",C_F,G_Y,U},
    {"てぃ",C_T,0,I},{"でぃ",C_D,0,I},{"とぅ",C_T,0,U},{"どぅ",C_D,0,U},
    {"てゅ",C_T,G_Y,U},{"でゅ",C_D,G_Y,U},
    {"すぃ",C_S,0,I},{"ずぃ",C_Z,0,I},
    {"つぁ",C_TS,0,A},{"つぃ",C_TS,0,I},{"つぇ",C_TS,0,E},{"つぉ",C_TS,0,O},
    {"うぃ",0,G_W,I},{"うぇ",0,G_W,E},{"うぉ",0,G_W,O},{"いぇ",0,G_Y,E},
    {"くぁ",C_K,G_W,A},{"くぃ",C_K,G_W,I},{"くぇ",C_K,G_W,E},{"くぉ",C_K,G_W,O},
    {"ぐぁ",C_G,G_W,A},
    {"ゔぁ",C_B,0,A},{"ゔぃ",C_B,0,I},{"ゔぇ",C_B,0,E},{"ゔぉ",C_B,0,O},{"ゔゅ",C_B,G_Y,U},
};

#undef A
#undef I
#undef U
#undef E
#undef O

/* UTF-8 の表項目を最大 2 コードポイントに展開 */
static int ent_decode(const char* s, uint32_t cp[2])
{
    const unsigned char* p = (const unsigned char*)s;
    int k = 0;
    while (*p && k < 2) {
        /* 表はすべて 3 バイト文字 */
        cp[k++] = ((uint32_t)(p[0] & 0x0F) << 12) | ((uint32_t)(p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        p += 3;
    }
    return *p ? 0 : k;
}

int ct_kana_lookup(const uint32_t* cp, int n, uint8_t* cons, uint8_t* glide, uint8_t* vowel)
{
    size_t i;
    int best = 0;
    for (i = 0; i < sizeof TABLE / sizeof TABLE[0]; i++) {
        uint32_t e[2];
        int len = ent_decode(TABLE[i].k, e);
        if (len <= best || len > n) continue;
        if (e[0] != cp[0]) continue;
        if (len == 2 && e[1] != cp[1]) continue;
        best = len;
        *cons = TABLE[i].c;
        *glide = TABLE[i].g;
        *vowel = TABLE[i].v;
    }
    return best;
}

/* 子音の調音様式・位置・有声性 */
static const CtConsInfo CONS[C_COUNT] = {
    /* C_NONE */ {MN_NONE,  PL_NONE, 1},
    /* C_K  */   {MN_STOP,  PL_VEL,  0},
    /* C_G  */   {MN_STOP,  PL_VEL,  1},
    /* C_S  */   {MN_FRIC,  PL_ALV,  0},
    /* C_SH */   {MN_FRIC,  PL_PAL,  0},
    /* C_Z  */   {MN_AFFR,  PL_ALV,  1},
    /* C_J  */   {MN_AFFR,  PL_PAL,  1},
    /* C_T  */   {MN_STOP,  PL_ALV,  0},
    /* C_D  */   {MN_STOP,  PL_ALV,  1},
    /* C_TS */   {MN_AFFR,  PL_ALV,  0},
    /* C_CH */   {MN_AFFR,  PL_PAL,  0},
    /* C_N  */   {MN_NASAL, PL_ALV,  1},
    /* C_H  */   {MN_FRIC,  PL_GLOT, 0},
    /* C_HY */   {MN_FRIC,  PL_PAL,  0},
    /* C_F  */   {MN_FRIC,  PL_LAB,  0},
    /* C_B  */   {MN_STOP,  PL_LAB,  1},
    /* C_P  */   {MN_STOP,  PL_LAB,  0},
    /* C_M  */   {MN_NASAL, PL_LAB,  1},
    /* C_R  */   {MN_FLAP,  PL_ALV,  1},
};

const CtConsInfo* ct_cons_info(int cons)
{
    if (cons < 0 || cons >= C_COUNT) cons = 0;
    return &CONS[cons];
}
