# colorful-talk

無償で使える日本語の規則音声合成エンジンです。C で書かれた小さなライブラリで、
カナとアクセント記号の「記号列」から WAV を作ります。

- 録音データを使わない **フォルマント合成**（Klatt 型カスケード／パラレル合成器を独自実装）なので、
  声データのライセンスを気にせず配布できます
- 辞書不要・依存ライブラリなし。DLL / 静的ライブラリ / CLI を同梱
- 声は 4 種類（女性・男性・子ども・ロボット）。話速・高さ・抑揚・音量を変更可能
- ライセンスは MIT

音声はいわゆる「ロボット声」です。録音ベースの商用エンジンほど自然ではありません。

## ダウンロード

ビルド済みの Windows (x64) 版は [Releases](https://github.com/ukagaka-irono/colorful-talk/releases) にあります。
`colorful_talk.dll` と `ctalk.exe` は VC ランタイムを含んでいるので、追加のインストールなしで動きます。

## ビルド

CMake 3.20 以上と C コンパイラ（Visual Studio 2019 以降 / gcc / clang）が必要です。

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release
```

```bash
ctest --test-dir build -C Release
```

成果物（Windows の場合 `build/Release/`）:

| ファイル | 内容 |
|---|---|
| `colorful_talk.dll` / `colorful_talk.lib` | DLL とインポートライブラリ |
| `colorful_talk_static.lib` | 静的ライブラリ（使う側で `CT_STATIC` を定義） |
| `ctalk.exe` | コマンドラインツール |

## コマンドライン

```bash
build/Release/ctalk.exe -o hello.wav "こんにちわ、せか'い。"
```

```bash
build/Release/ctalk.exe --play -v m1 -s 120 "きょ'うわ/い'い/てんきです。"
```

| オプション | 意味 |
|---|---|
| `-o FILE` | WAV の出力先（省略時 `out.wav`、`--play` のみなら書き出さない） |
| `-f FILE` | 記号列を UTF-8 のテキストファイルから読む |
| `-v VOICE` | `f1`（女性）/ `m1`（男性）/ `child` / `robot` |
| `-s N` | 話速 50–300（100 が標準） |
| `-p N` | 声の高さ 50–200 |
| `-a N` | 抑揚 0–200（0 で平坦） |
| `-g N` | 音量 0–200 |
| `-r N` | サンプリング周波数 8000–48000（既定 16000） |
| `--play` | その場で再生（Windows） |

## ライブラリとして使う

```c
#include "colorful_talk.h"

CTalkParams p;
int size, err;
CTalk_DefaultParams(&p);
p.voice = CT_VOICE_F1;
p.speed = 110;

unsigned char* wav = CTalk_Synthe("こんにちわ、せか'い。", &p, &size, &err);
if (wav) {
    /* wav[0..size) が RIFF/WAVE 形式（16bit, モノラル） */
    CTalk_FreeWave(wav);
} else {
    printf("%s (文字位置 %d)\n", CTalk_ErrorString(err), CTalk_ErrorPos());
}
```

PCM だけ欲しい場合は `CTalk_SynthePCM()` を使います。API の詳細は
[include/colorful_talk.h](include/colorful_talk.h) を参照してください。
すべての関数はスレッドセーフです。

C 以外からは DLL を読み込んで呼べます（呼出規約は `cdecl`、文字列は UTF-8）。

```python
import ctypes
ct = ctypes.CDLL("colorful_talk.dll")
ct.CTalk_Synthe.restype = ctypes.c_void_p
size, err = ctypes.c_int(), ctypes.c_int()
buf = ct.CTalk_Synthe("こんにちわ".encode(), None, ctypes.byref(size), ctypes.byref(err))
wav = ctypes.string_at(buf, size.value)
ct.CTalk_FreeWave(ctypes.c_void_p(buf))
```

## 記号列

読みはカナで、アクセント核を `'`、句の区切りを `/` `、` `。` `？` で書きます。
詳しくは [docs/koe-format.md](docs/koe-format.md)。

```
こんにちわ、せか'い。
きょ'うわ/い'い/てんきです。
ほんとうに/そ'うですか？
```

## 仕組み

```
記号列 ─ parser ─▶ モーラ列＋アクセント句 ─ prosody ─▶ 音素セグメント（長さ）＋F0 目標点
      ─ phoneme/track ─▶ 2ms ごとの合成パラメータ ─ klatt ─▶ 波形 ─ wav ─▶ WAV
```

| ファイル | 役割 |
|---|---|
| `src/parser.c`, `src/kana_table.c` | 記号列の解析、カナ → 音素、母音の無声化 |
| `src/prosody.c` | 音素の長さ、ポーズ、東京方言アクセントによる F0 |
| `src/phoneme.c` | 母音のフォルマント、子音のローカス・雑音形状・鼻音 |
| `src/track.c` | パラメータ軌跡の補間と平滑化（調音結合） |
| `src/klatt.c` | 声帯音源＋カスケード／パラレル フォルマント合成器 |

声の調整は `src/phoneme.c`（音色）、`src/prosody.c`（長さ・抑揚）、
`src/api.c` の `VOICES` 表（声の種類）で行えます。`samples/make_samples.ps1` で確認用 WAV を作れます。

## ライセンス

MIT License。詳細は [LICENSE](LICENSE)。
