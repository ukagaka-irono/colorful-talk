/* 合成のテスト（公開 API 経由） */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "colorful_talk.h"

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static const char* ALL_MORA[] = {
    "あ","い","う","え","お","か","き","く","け","こ","が","ぎ","ぐ","げ","ご",
    "さ","し","す","せ","そ","ざ","じ","ず","ぜ","ぞ","た","ち","つ","て","と",
    "だ","ぢ","づ","で","ど","な","に","ぬ","ね","の","は","ひ","ふ","へ","ほ",
    "ば","び","ぶ","べ","ぼ","ぱ","ぴ","ぷ","ぺ","ぽ","ま","み","む","め","も",
    "や","ゆ","よ","ら","り","る","れ","ろ","わ","を","ん","ゔ",
    "きゃ","きゅ","きょ","ぎゃ","しゃ","しゅ","しょ","じゃ","ちゃ","ちゅ","ちょ",
    "にゃ","ひゃ","びゃ","ぴゃ","みゃ","りゃ","ふぁ","てぃ","でぃ","とぅ","うぃ","うぇ","うぉ",
    "あっか","あった","あっさ","あっぱ","かーど","さんぽ","さんか","ほんや",
};

static double rms(const short* x, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; i++) s += (double)x[i] * x[i];
    return n ? sqrt(s / n) : 0;
}

/* 自己相関による F0 推定（[from, from+len) の区間） */
static double estimate_f0(const short* x, int n, int fs, int from, int len)
{
    int lag, best = 0, i, lo = fs / 500, hi = fs / 60;
    double bestv = 0, e0 = 0;
    if (from < 0 || from + len + hi > n) return 0;
    for (i = 0; i < len; i++) e0 += (double)x[from + i] * x[from + i];
    for (lag = lo; lag <= hi; lag++) {
        double s = 0, e1 = 0;
        for (i = 0; i < len; i++) {
            s += (double)x[from + i] * x[from + i + lag];
            e1 += (double)x[from + i + lag] * x[from + i + lag];
        }
        s /= sqrt(e0 * e1) + 1e-9;
        if (s > bestv) { bestv = s; best = lag; }
    }
    return best && bestv > 0.5 ? (double)fs / best : 0;
}

/* 末尾 ~100ms の中で推定できた最大の F0 */
static double max_f0_near_end(const short* x, int n, int fs, int end, int w)
{
    double best = 0;
    int from;
    for (from = end - 4 * w; from <= end - w; from += w / 3) {
        double f = estimate_f0(x, n, fs, from, w);
        if (f > best) best = f;
    }
    return best;
}

/* 有音区間（|x| がしきい値を超える最初と最後） */
static void voiced_span(const short* x, int n, int* a, int* b)
{
    int i;
    *a = 0; *b = n;
    for (i = 0; i < n; i++) if (abs(x[i]) > 1500) { *a = i; break; }
    for (i = n - 1; i >= 0; i--) if (abs(x[i]) > 1500) { *b = i; break; }
}

static void test_every_mora(void)
{
    size_t k;
    for (k = 0; k < sizeof ALL_MORA / sizeof ALL_MORA[0]; k++) {
        short* pcm = NULL;
        int n = 0, fs = 0, i, clip = 0;
        int err = CTalk_SynthePCM(ALL_MORA[k], NULL, &pcm, &n, &fs);
        double r;
        if (err) { printf("FAIL %s: err %d\n", ALL_MORA[k], err); failures++; continue; }
        r = rms(pcm, n);
        for (i = 0; i < n; i++) if (pcm[i] >= 32000 || pcm[i] <= -32000) clip++;
        if (r < 300 || clip > n / 1000) {
            printf("FAIL %s: rms=%.0f clipped=%d/%d\n", ALL_MORA[k], r, clip, n);
            failures++;
        }
        CTalk_FreeWave(pcm);
    }
}

static void test_wav_header(void)
{
    int size = 0, err = -1;
    unsigned char* w = CTalk_Synthe("こんにちわ", NULL, &size, &err);
    CHECK(err == CT_OK && w != NULL);
    if (!w) return;
    CHECK(size > 44);
    CHECK(memcmp(w, "RIFF", 4) == 0 && memcmp(w + 8, "WAVE", 4) == 0);
    CHECK(memcmp(w + 36, "data", 4) == 0);
    CHECK((w[24] | (w[25] << 8) | (w[26] << 16)) == 16000);
    CHECK((int)(w[40] | (w[41] << 8) | (w[42] << 16) | ((unsigned)w[43] << 24)) == size - 44);
    CTalk_FreeWave(w);
}

static void test_speed(void)
{
    CTalkParams p;
    short *a = NULL, *b = NULL;
    int na = 0, nb = 0;
    CTalk_DefaultParams(&p);
    CHECK(CTalk_SynthePCM("あいうえお、かきくけこ", &p, &a, &na, NULL) == CT_OK);
    p.speed = 200;
    CHECK(CTalk_SynthePCM("あいうえお、かきくけこ", &p, &b, &nb, NULL) == CT_OK);
    if (a && b) {
        double ratio = (double)nb / na;
        if (ratio < 0.45 || ratio > 0.65) { printf("FAIL speed ratio %.2f\n", ratio); failures++; }
    }
    CTalk_FreeWave(a);
    CTalk_FreeWave(b);
}

static void test_sample_rates(void)
{
    static const int rates[] = { 8000, 11025, 22050, 44100, 48000 };
    size_t k;
    for (k = 0; k < sizeof rates / sizeof rates[0]; k++) {
        CTalkParams p;
        short* pcm = NULL;
        int n = 0, fs = 0;
        CTalk_DefaultParams(&p);
        p.sample_rate = rates[k];
        CHECK(CTalk_SynthePCM("さしすせそ", &p, &pcm, &n, &fs) == CT_OK);
        CHECK(fs == rates[k]);
        if (pcm && rms(pcm, n) < 300) { printf("FAIL rate %d silent\n", rates[k]); failures++; }
        CTalk_FreeWave(pcm);
    }
}

static void test_pitch_and_accent(void)
{
    CTalkParams p;
    short* pcm = NULL;
    int n = 0, fs = 0, a, b, len;
    double f;
    CTalk_DefaultParams(&p);
    p.voice = CT_VOICE_M1;

    /* 基準ピッチ */
    CHECK(CTalk_SynthePCM("あー", &p, &pcm, &n, &fs) == CT_OK);
    voiced_span(pcm, n, &a, &b);
    len = fs / 20;
    f = estimate_f0(pcm, n, fs, (a + b) / 2 - len / 2, len);
    if (f < 100 || f > 150) { printf("FAIL M1 f0 %.1f Hz\n", f); failures++; }
    CTalk_FreeWave(pcm);

    /* 頭高「あ'い」は前が高く、尾高「あい'」は後が高い */
    {
        static const char* words[2] = { "あ'い", "あい'" };
        double f1[2], f2[2];
        int w;
        for (w = 0; w < 2; w++) {
            CHECK(CTalk_SynthePCM(words[w], &p, &pcm, &n, &fs) == CT_OK);
            voiced_span(pcm, n, &a, &b);
            f1[w] = estimate_f0(pcm, n, fs, a + (b - a) / 4 - len / 2, len);
            f2[w] = estimate_f0(pcm, n, fs, a + (b - a) * 2 / 3 - len / 2, len);
            CTalk_FreeWave(pcm);
        }
        printf("accent: あ'い %.0f -> %.0f Hz, あい' %.0f -> %.0f Hz\n", f1[0], f2[0], f1[1], f2[1]);
        CHECK(f1[0] > f2[0] * 1.08);
        CHECK(f2[1] > f1[1] * 1.08);
    }

    /* 疑問文は末尾が上がる */
    {
        double fq, fs_;
        int w = fs / 33;   /* 30ms 窓、末尾の 30ms 手前 */
        CHECK(CTalk_SynthePCM("そうですか？", &p, &pcm, &n, &fs) == CT_OK);
        voiced_span(pcm, n, &a, &b);
        fq = max_f0_near_end(pcm, n, fs, b, w);
        CTalk_FreeWave(pcm);
        CHECK(CTalk_SynthePCM("そうですか。", &p, &pcm, &n, &fs) == CT_OK);
        voiced_span(pcm, n, &a, &b);
        fs_ = max_f0_near_end(pcm, n, fs, b, w);
        CTalk_FreeWave(pcm);
        printf("question end %.0f Hz, statement end %.0f Hz\n", fq, fs_);
        CHECK(fq > fs_ * 1.2);
    }
}

static void test_errors(void)
{
    CTalkParams p;
    short* pcm = NULL;
    int n = 0, err = 0, size = 0;
    CHECK(CTalk_SynthePCM("あいう漢字", NULL, &pcm, &n, NULL) == CT_ERR_CHAR);
    CHECK(CTalk_ErrorPos() == 3);
    CHECK(pcm == NULL);
    CHECK(CTalk_Synthe(NULL, NULL, &size, &err) == NULL && err == CT_ERR_NULL);
    CTalk_DefaultParams(&p);
    p.speed = 10;
    CHECK(CTalk_Synthe("あ", &p, &size, &err) == NULL && err == CT_ERR_PARAM);
    CHECK(strcmp(CTalk_ErrorString(CT_ERR_CHAR), "unknown error") != 0);
}

static void test_voices(void)
{
    int v;
    for (v = 0; v < CT_VOICE_COUNT; v++) {
        CTalkParams p;
        short* pcm = NULL;
        int n = 0;
        CTalk_DefaultParams(&p);
        p.voice = v;
        CHECK(CTalk_SynthePCM("こんにちわ、せか'い。", &p, &pcm, &n, NULL) == CT_OK);
        if (pcm && rms(pcm, n) < 500) { printf("FAIL voice %d too quiet\n", v); failures++; }
        CTalk_FreeWave(pcm);
    }
}

int main(void)
{
    test_every_mora();
    test_wav_header();
    test_speed();
    test_sample_rates();
    test_pitch_and_accent();
    test_errors();
    test_voices();
    if (failures) { printf("%d failure(s)\n", failures); return 1; }
    printf("synth: all tests passed\n");
    return 0;
}
