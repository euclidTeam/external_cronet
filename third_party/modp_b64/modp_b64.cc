/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 4 -*- */
/* vi: set expandtab shiftwidth=4 tabstop=4: */
/**
 * \file
 * <PRE>
 * MODP_B64 - High performance base64 encoder/decoder
 * Version 1.3 -- 17-Mar-2006
 * http://modp.com/release/base64
 *
 * Copyright &copy; 2005, 2006  Nick Galbreath -- nickg [at] modp [dot] com
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 *   Neither the name of the modp.com nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This is the standard "new" BSD license:
 * http://www.opensource.org/licenses/bsd-license.php
 * </PRE>
 */

/* public header */
#include "modp_b64.h"

#include "modp_b64_data.h"

#define BADCHAR 0x01FFFFFF

size_t modp_b64_encode_data(char* dest, const char* str, size_t len)
{
    size_t i = 0;
    uint8_t* p = (uint8_t*) dest;

    /* unsigned here is important! */
    uint8_t t1, t2, t3;

    if (len > 2) {
        for (; i < len - 2; i += 3) {
            t1 = str[i]; t2 = str[i+1]; t3 = str[i+2];
            *p++ = e0[t1];
            *p++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
            *p++ = e1[((t2 & 0x0F) << 2) | ((t3 >> 6) & 0x03)];
            *p++ = e2[t3];
        }
    }

    switch (len - i) {
    case 0:
        break;
    case 1:
        t1 = str[i];
        *p++ = e0[t1];
        *p++ = e1[(t1 & 0x03) << 4];
        *p++ = CHARPAD;
        *p++ = CHARPAD;
        break;
    default: /* case 2 */
        t1 = str[i]; t2 = str[i+1];
        *p++ = e0[t1];
        *p++ = e1[((t1 & 0x03) << 4) | ((t2 >> 4) & 0x0F)];
        *p++ = e2[(t2 & 0x0F) << 2];
        *p++ = CHARPAD;
    }

    return p - (uint8_t*)dest;
}

size_t modp_b64_encode(char* dest, const char* str, size_t len) {
  size_t output_size = modp_b64_encode_data(dest, str, len);
  dest[output_size] = '\0';
  return output_size;
}

size_t do_decode_padding(const char* src, size_t len, ModpDecodePolicy policy) {
  if (policy == ModpDecodePolicy::kNoPaddingValidation) {
    while (len > 0 && src[len - 1] == CHARPAD) {
        len--;
    }
  } else {
    const size_t remainder = len % 4;
    if (policy == ModpDecodePolicy::kStrict && (remainder != 0 || len < 4))
      return MODP_B64_ERROR;
    if (remainder == 0) {
      if (src[len - 1] == CHARPAD) {
        len--;
        if (src[len - 1] == CHARPAD) {
          len--;
        }
      }
    }
  }
  return len % 4 == 1 ? MODP_B64_ERROR : len;
}

size_t modp_b64_decode(char* dest, const char* src, size_t len, ModpDecodePolicy policy)
{
    if (len != 0)
      len = do_decode_padding(src, len, policy);

    if (len == 0 || len == MODP_B64_ERROR)
      return len;

    size_t i;
    int leftover = len % 4;
    size_t chunks = (leftover == 0) ? len / 4 - 1 : len /4;

    uint8_t* p = (uint8_t*)dest;
    uint32_t x = 0;
    const uint8_t* y = (uint8_t*)src;
    for (i = 0; i < chunks; ++i, y += 4) {
        x = d0[y[0]] | d1[y[1]] | d2[y[2]] | d3[y[3]];
        if (x >= BADCHAR) return MODP_B64_ERROR;
        *p++ =  ((uint8_t*)(&x))[0];
        *p++ =  ((uint8_t*)(&x))[1];
        *p++ =  ((uint8_t*)(&x))[2];
    }

    switch (leftover) {
    case 0:
        x = d0[y[0]] | d1[y[1]] | d2[y[2]] | d3[y[3]];

        if (x >= BADCHAR) return MODP_B64_ERROR;
        *p++ =  ((uint8_t*)(&x))[0];
        *p++ =  ((uint8_t*)(&x))[1];
        *p =    ((uint8_t*)(&x))[2];
        return (chunks+1)*3;
    case 1:  /* with padding this is an impossible case */
        x = d0[y[0]];
        *p = *((uint8_t*)(&x)); // i.e. first char/byte in int
        break;
    case 2: // * case 2, 1  output byte */
        x = d0[y[0]] | d1[y[1]];
        *p = *((uint8_t*)(&x)); // i.e. first char
        break;
    default: /* case 3, 2 output bytes */
        x = d0[y[0]] | d1[y[1]] | d2[y[2]];  /* 0x3c */
        *p++ =  ((uint8_t*)(&x))[0];
        *p =  ((uint8_t*)(&x))[1];
        break;
    }

    if (x >= BADCHAR) return MODP_B64_ERROR;

    return 3*chunks + (6*leftover)/8;
}
