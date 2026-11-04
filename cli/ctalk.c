/* ctalk: colorful-talk コマンドラインツール  SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "colorful_talk.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <mmsystem.h>
#endif

static void usage(void)
{
    fputs(
        "usage: ctalk [options] \"koe string\"\n"
        "  -o FILE    write WAV to FILE (default: out.wav, unless --play)\n"
        "  -f FILE    read koe string from a UTF-8 text file\n"
        "  -v VOICE   f1 | m1 | child | robot   (default f1)\n"
        "  -s N       speed 50-300   (default 100)\n"
        "  -p N       pitch 50-200   (default 100)\n"
        "  -a N       accent 0-200   (default 100)\n"
        "  -g N       volume 0-200   (default 100)\n"
        "  -r N       sample rate 8000-48000 (default 16000)\n"
        "  --play     play the result\n"
        "example: ctalk -o hello.wav \"こんにちわ、せか'い。\"\n",
        stderr);
}

static FILE* u8fopen(const char* path, const char* mode)
{
#ifdef _WIN32
    wchar_t wp[1024], wm[8];
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wp, 1024)) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, mode, -1, wm, 8)) return NULL;
    return _wfopen(wp, wm);
#else
    return fopen(path, mode);
#endif
}

static char* read_file(const char* path)
{
    FILE* f = u8fopen(path, "rb");
    char* buf;
    long len;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char*)malloc((size_t)len + 1);
    if (!buf) { fclose(f); return NULL; }
    len = (long)fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);
    if ((unsigned char)buf[0] == 0xEF && (unsigned char)buf[1] == 0xBB && (unsigned char)buf[2] == 0xBF)
        memmove(buf, buf + 3, (size_t)len - 2);
    return buf;
}

static int parse_voice(const char* s)
{
    if (!strcmp(s, "f1") || !strcmp(s, "0")) return CT_VOICE_F1;
    if (!strcmp(s, "m1") || !strcmp(s, "1")) return CT_VOICE_M1;
    if (!strcmp(s, "child") || !strcmp(s, "2")) return CT_VOICE_CHILD;
    if (!strcmp(s, "robot") || !strcmp(s, "3")) return CT_VOICE_ROBOT;
    return -1;
}

/* エラー位置の文字を表示する */
static void print_error(const char* koe, int err)
{
    int pos = CTalk_ErrorPos();
    fprintf(stderr, "ctalk: error %d: %s", err, CTalk_ErrorString(err));
    if (pos >= 0 && koe) {
        const unsigned char* p = (const unsigned char*)koe;
        int k = 0;
        while (*p && k < pos) {
            p++;
            while ((*p & 0xC0) == 0x80) p++;
            k++;
        }
        if (*p) {
            const unsigned char* q = p + 1;
            while ((*q & 0xC0) == 0x80) q++;
            fprintf(stderr, " at character %d '%.*s'", pos, (int)(q - p), (const char*)p);
        } else {
            fprintf(stderr, " at character %d", pos);
        }
    }
    fputc('\n', stderr);
}

static int run(int argc, char** argv)
{
    CTalkParams prm;
    const char* out = NULL;
    char* koe = NULL;
    int owned = 0, play = 0, i, size = 0, err = 0;
    unsigned char* wav;

    CTalk_DefaultParams(&prm);
    for (i = 1; i < argc; i++) {
        const char* a = argv[i];
        const char* val = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(); return 0; }
        if (!strcmp(a, "--play")) { play = 1; continue; }
        if (a[0] == '-' && a[1] && !a[2] && strchr("ofvspagr", a[1])) {
            if (!val) { usage(); return 2; }
            i++;
            switch (a[1]) {
            case 'o': out = val; break;
            case 'f':
                koe = read_file(val);
                if (!koe) { fprintf(stderr, "ctalk: cannot read %s\n", val); return 1; }
                owned = 1;
                break;
            case 'v':
                prm.voice = parse_voice(val);
                if (prm.voice < 0) { fprintf(stderr, "ctalk: unknown voice %s\n", val); return 2; }
                break;
            case 's': prm.speed = atoi(val); break;
            case 'p': prm.pitch = atoi(val); break;
            case 'a': prm.accent = atoi(val); break;
            case 'g': prm.volume = atoi(val); break;
            case 'r': prm.sample_rate = atoi(val); break;
            }
            continue;
        }
        if (koe) { usage(); return 2; }
        koe = argv[i];
    }
    if (!koe) { usage(); return 2; }

    wav = CTalk_Synthe(koe, &prm, &size, &err);
    if (!wav) {
        print_error(koe, err);
        if (owned) free(koe);
        return 1;
    }
    if (owned) free(koe);

    if (out || !play) {
        FILE* f = u8fopen(out ? out : "out.wav", "wb");
        if (!f || fwrite(wav, 1, (size_t)size, f) != (size_t)size) {
            fprintf(stderr, "ctalk: cannot write %s\n", out ? out : "out.wav");
            if (f) fclose(f);
            CTalk_FreeWave(wav);
            return 1;
        }
        fclose(f);
    }
    if (play) {
#ifdef _WIN32
        PlaySoundA((LPCSTR)wav, NULL, SND_MEMORY | SND_SYNC);
#else
        fputs("ctalk: --play is only supported on Windows\n", stderr);
#endif
    }
    CTalk_FreeWave(wav);
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t** wargv)
{
    char** argv = (char**)calloc((size_t)argc + 1, sizeof *argv);
    int i, r;
    SetConsoleOutputCP(CP_UTF8);
    for (i = 0; i < argc; i++) {
        int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, NULL, 0, NULL, NULL);
        argv[i] = (char*)malloc((size_t)len);
        WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, argv[i], len, NULL, NULL);
    }
    r = run(argc, argv);
    for (i = 0; i < argc; i++) free(argv[i]);
    free(argv);
    return r;
}
#else
int main(int argc, char** argv)
{
    return run(argc, argv);
}
#endif
