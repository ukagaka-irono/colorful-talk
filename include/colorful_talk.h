/*
 * colorful-talk : 日本語 規則音声合成エンジン（フォルマント合成）
 * SPDX-License-Identifier: MIT
 *
 * 入力はカナ＋アクセント記号の「記号列」（UTF-8）。仕様は docs/koe-format.md を参照。
 * すべての関数はスレッドセーフ（グローバルな可変状態を持たない）。
 */
#ifndef COLORFUL_TALK_H
#define COLORFUL_TALK_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(CT_STATIC)
#    define CT_API
#  elif defined(CT_BUILD_DLL)
#    define CT_API __declspec(dllexport)
#  else
#    define CT_API __declspec(dllimport)
#  endif
#  define CT_CALL __cdecl
#else
#  if defined(CT_BUILD_DLL)
#    define CT_API __attribute__((visibility("default")))
#  else
#    define CT_API
#  endif
#  define CT_CALL
#endif

#define CTALK_VERSION "1.0.0"

/* 声の種類 */
enum {
    CT_VOICE_F1    = 0, /* 女性 */
    CT_VOICE_M1    = 1, /* 男性 */
    CT_VOICE_CHILD = 2, /* 子ども */
    CT_VOICE_ROBOT = 3, /* ロボット */
    CT_VOICE_COUNT = 4
};

/* エラーコード */
enum {
    CT_OK           = 0,
    CT_ERR_NULL     = 1,  /* 引数が NULL */
    CT_ERR_UTF8     = 2,  /* 不正な UTF-8 */
    CT_ERR_CHAR     = 3,  /* 記号列に使えない文字 */
    CT_ERR_ACCENT   = 4,  /* アクセント記号 ' の位置が不正 */
    CT_ERR_LONG     = 5,  /* 長音 ー の位置が不正 */
    CT_ERR_DEVOICE  = 6,  /* 無声化記号 _ の位置が不正 */
    CT_ERR_EMPTY    = 7,  /* 発音する文字がない */
    CT_ERR_PARAM    = 8,  /* パラメータが範囲外 */
    CT_ERR_TOOLONG  = 9,  /* 記号列が長すぎる */
    CT_ERR_MEMORY   = 10  /* メモリ不足 */
};

typedef struct {
    int voice;       /* CT_VOICE_*                          既定 CT_VOICE_F1 */
    int speed;       /* 話速 50–300 (100 = 標準)             既定 100 */
    int pitch;       /* 声の高さ 50–200 (100 = 声の基準)      既定 100 */
    int accent;      /* 抑揚の強さ 0–200 (0 = 平坦)          既定 100 */
    int volume;      /* 音量 0–200                           既定 100 */
    int sample_rate; /* 8000–48000 Hz、0 なら 16000          既定 0 */
} CTalkParams;

/* パラメータを既定値で埋める */
CT_API void CT_CALL CTalk_DefaultParams(CTalkParams* p);

/*
 * 記号列から WAV ファイル（RIFF, 16bit PCM, モノラル）のバイト列を作る。
 * params が NULL なら既定値。成功時はバッファを返し *size にバイト数を入れる。
 * 失敗時は NULL を返し *err にエラーコードを入れる（err は NULL 可）。
 * 返したバッファは CTalk_FreeWave で解放すること。
 */
CT_API unsigned char* CT_CALL CTalk_Synthe(const char* koe_utf8, const CTalkParams* params,
                                           int* size, int* err);

/*
 * 生の PCM（16bit 符号付き, モノラル）を返す版。戻り値はエラーコード。
 * 成功時 *pcm を CTalk_FreeWave で解放すること。サンプリング周波数は *sample_rate に入る（NULL 可）。
 */
CT_API int CT_CALL CTalk_SynthePCM(const char* koe_utf8, const CTalkParams* params,
                                   short** pcm, int* samples, int* sample_rate);

/* CTalk_Synthe / CTalk_SynthePCM が返したバッファを解放する（NULL 可） */
CT_API void CT_CALL CTalk_FreeWave(void* buf);

/* エラーコードの説明（英語, 静的文字列） */
CT_API const char* CT_CALL CTalk_ErrorString(int err);

/* このスレッドで直近に失敗した呼び出しのエラー位置（記号列の何文字目か, 0 始まり）。不明なら -1 */
CT_API int CT_CALL CTalk_ErrorPos(void);

/* バージョン文字列 */
CT_API const char* CT_CALL CTalk_Version(void);

#ifdef __cplusplus
}
#endif

#endif /* COLORFUL_TALK_H */
