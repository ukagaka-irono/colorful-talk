/* 記号列パーサのテスト */
#include <stdio.h>
#include <string.h>
#include "ct_internal.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static int parse(const char* s, CtUtterance* u, int* pos)
{
    return ct_parse(s, u, pos);
}

static void expect_error(const char* s, int err, int pos)
{
    CtUtterance u;
    int p, e = parse(s, &u, &p);
    if (e != err || p != pos) {
        printf("FAIL \"%s\": got err=%d pos=%d, want err=%d pos=%d\n", s, e, p, err, pos);
        failures++;
    }
    if (e == CT_OK) ct_utterance_free(&u);
}

static void test_basic(void)
{
    CtUtterance u;
    int pos;
    CHECK(parse("こんにちわ", &u, &pos) == CT_OK);
    CHECK(u.n_mora == 5);
    CHECK(u.mora[0].cons == C_K && u.mora[0].vowel == V_O);
    CHECK(u.mora[1].type == M_N);
    CHECK(u.mora[2].cons == C_N && u.mora[2].vowel == V_I);
    CHECK(u.mora[3].cons == C_CH && u.mora[3].vowel == V_I);
    CHECK(u.mora[4].glide == G_W && u.mora[4].vowel == V_A);
    CHECK(u.n_phrase == 1 && u.phrase[0].accent == 0 && u.phrase[0].end == PE_PERIOD);
    ct_utterance_free(&u);
}

static void test_katakana_and_youon(void)
{
    CtUtterance u;
    int pos;
    CHECK(parse("キャット", &u, &pos) == CT_OK);
    CHECK(u.n_mora == 3);
    CHECK(u.mora[0].cons == C_K && u.mora[0].glide == G_Y && u.mora[0].vowel == V_A);
    CHECK(u.mora[1].type == M_Q);
    CHECK(u.mora[2].cons == C_T && u.mora[2].vowel == V_O);
    ct_utterance_free(&u);

    CHECK(parse("ふぁいてぃんぐ", &u, &pos) == CT_OK);
    CHECK(u.n_mora == 5);
    CHECK(u.mora[0].cons == C_F && u.mora[0].vowel == V_A);
    CHECK(u.mora[2].cons == C_T && u.mora[2].vowel == V_I);
    CHECK(u.mora[3].type == M_N);
    CHECK(u.mora[4].cons == C_G && u.mora[4].vowel == V_U);
    ct_utterance_free(&u);
}

static void test_long_vowel(void)
{
    CtUtterance u;
    int pos;
    CHECK(parse("らーめん", &u, &pos) == CT_OK);
    CHECK(u.n_mora == 4);
    CHECK(u.mora[1].type == M_LONG && u.mora[1].vowel == V_A);
    ct_utterance_free(&u);
    CHECK(parse("ん～", &u, &pos) == CT_OK);
    CHECK(u.n_mora == 2 && u.mora[1].type == M_N);
    ct_utterance_free(&u);
}

static void test_accent_and_phrases(void)
{
    CtUtterance u;
    int pos;
    CHECK(parse("は'し", &u, &pos) == CT_OK);
    CHECK(u.phrase[0].accent == 1);
    ct_utterance_free(&u);
    CHECK(parse("はし'", &u, &pos) == CT_OK);
    CHECK(u.phrase[0].accent == 2);
    ct_utterance_free(&u);

    CHECK(parse("あ/い、う。え？", &u, &pos) == CT_OK);
    CHECK(u.n_phrase == 4);
    CHECK(u.phrase[0].end == PE_NONE);
    CHECK(u.phrase[1].end == PE_COMMA);
    CHECK(u.phrase[2].end == PE_PERIOD);
    CHECK(u.phrase[3].end == PE_QUESTION);
    CHECK(u.mora[3].phrase == 3);
    ct_utterance_free(&u);

    /* 全角記号・空白 */
    CHECK(parse("きょ＇うわ　いい／てんき．", &u, &pos) == CT_OK);
    CHECK(u.n_phrase == 2 && u.phrase[0].accent == 1 && u.phrase[0].count == 5);
    ct_utterance_free(&u);
}

static void test_devoice(void)
{
    CtUtterance u;
    int pos;
    CHECK(parse("です。", &u, &pos) == CT_OK);
    CHECK(u.mora[1].devoice == 1);
    ct_utterance_free(&u);

    CHECK(parse("です？", &u, &pos) == CT_OK);
    CHECK(u.mora[1].devoice == 0);
    ct_utterance_free(&u);

    CHECK(parse("きた", &u, &pos) == CT_OK);
    CHECK(u.mora[0].devoice == 1);
    ct_utterance_free(&u);

    CHECK(parse("き'た", &u, &pos) == CT_OK);   /* アクセント核は無声化しない */
    CHECK(u.mora[0].devoice == 0);
    ct_utterance_free(&u);

    CHECK(parse("_あ", &u, &pos) == CT_OK);
    CHECK(u.mora[0].devoice == 1 && u.mora[0].forced == 1);
    ct_utterance_free(&u);
}

static void test_errors(void)
{
    expect_error("", CT_ERR_EMPTY, 0);
    expect_error("、。", CT_ERR_EMPTY, 0);
    expect_error("'あ", CT_ERR_ACCENT, 0);
    expect_error("あ''", CT_ERR_ACCENT, 2);
    expect_error("あ'い'", CT_ERR_ACCENT, 3);
    expect_error("あxい", CT_ERR_CHAR, 1);
    expect_error("あ漢", CT_ERR_CHAR, 1);
    expect_error("ーあ", CT_ERR_LONG, 0);
    expect_error("あっー", CT_ERR_LONG, 2);
    expect_error("あ_", CT_ERR_DEVOICE, 1);
    expect_error("_ん", CT_ERR_DEVOICE, 0);
    expect_error("あ\xff", CT_ERR_UTF8, 1);
}

int main(void)
{
    test_basic();
    test_katakana_and_youon();
    test_long_vowel();
    test_accent_and_phrases();
    test_devoice();
    test_errors();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("parser: all tests passed\n");
    return 0;
}
