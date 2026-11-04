/* UTF-8 デコード  SPDX-License-Identifier: MIT */
#include <stdlib.h>
#include <string.h>
#include "ct_internal.h"

int ct_utf8_decode(const char* s, uint32_t** out, int* n)
{
    size_t len = strlen(s);
    const unsigned char* p = (const unsigned char*)s;
    uint32_t* cp = (uint32_t*)malloc((len + 1) * sizeof *cp);
    int k = 0;
    *out = NULL;
    *n = 0;
    if (!cp) return CT_ERR_MEMORY;

    while (*p) {
        uint32_t c;
        int extra, i;
        if (*p < 0x80)              { c = *p;        extra = 0; }
        else if ((*p & 0xE0) == 0xC0) { c = *p & 0x1F; extra = 1; }
        else if ((*p & 0xF0) == 0xE0) { c = *p & 0x0F; extra = 2; }
        else if ((*p & 0xF8) == 0xF0) { c = *p & 0x07; extra = 3; }
        else goto bad;
        p++;
        for (i = 0; i < extra; i++, p++) {
            if ((*p & 0xC0) != 0x80) goto bad;
            c = (c << 6) | (*p & 0x3F);
        }
        if ((extra == 1 && c < 0x80) || (extra == 2 && c < 0x800) ||
            (extra == 3 && (c < 0x10000 || c > 0x10FFFF)) ||
            (c >= 0xD800 && c <= 0xDFFF))
            goto bad;
        cp[k++] = c;
    }
    *out = cp;
    *n = k;
    return CT_OK;

bad:
    free(cp);
    *n = k;
    return CT_ERR_UTF8;
}
