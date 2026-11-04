/* WAV (RIFF, 16bit PCM, mono) 書き出し  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "ct_internal.h"

static void put32(unsigned char* p, uint32_t v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void put16(unsigned char* p, uint16_t v)
{
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}

unsigned char* ct_wav_build(const short* pcm, int n, int fs, int* size)
{
    uint32_t data = (uint32_t)n * 2u;
    unsigned char* w = (unsigned char*)malloc(44 + (size_t)data);
    int i;
    if (!w) return NULL;
    memcpy(w, "RIFF", 4);  put32(w + 4, 36 + data);
    memcpy(w + 8, "WAVE", 4);
    memcpy(w + 12, "fmt ", 4); put32(w + 16, 16);
    put16(w + 20, 1);              /* PCM */
    put16(w + 22, 1);              /* mono */
    put32(w + 24, (uint32_t)fs);
    put32(w + 28, (uint32_t)fs * 2u);
    put16(w + 32, 2);
    put16(w + 34, 16);
    memcpy(w + 36, "data", 4); put32(w + 40, data);
    for (i = 0; i < n; i++) put16(w + 44 + 2 * i, (uint16_t)pcm[i]);
    *size = (int)(44 + data);
    return w;
}
