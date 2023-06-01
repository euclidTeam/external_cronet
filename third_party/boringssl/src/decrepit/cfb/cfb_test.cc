// Copyright (c) 2017, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#include <openssl/cipher.h>

#include <gtest/gtest.h>

#include "../../crypto/internal.h"
#include "../../crypto/test/test_util.h"

struct CFBTestCase {
  size_t key_len;
  uint8_t key[32];
  uint8_t iv[16];
  uint8_t plaintext[16*4];
  uint8_t ciphertext[16*4];
};

static const CFBTestCase kCFBTestCases[] = {
  {
    // This is the test case from
    // http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf,
    // section F.3.13, for CFB128-AES128
    16,
    {0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6, 0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
    {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
     0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
     0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
     0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10},
    {0x3b, 0x3f, 0xd9, 0x2e, 0xb7, 0x2d, 0xad, 0x20, 0x33, 0x34, 0x49, 0xf8, 0xe8, 0x3c, 0xfb, 0x4a,
     0xc8, 0xa6, 0x45, 0x37, 0xa0, 0xb3, 0xa9, 0x3f, 0xcd, 0xe3, 0xcd, 0xad, 0x9f, 0x1c, 0xe5, 0x8b,
     0x26, 0x75, 0x1f, 0x67, 0xa3, 0xcb, 0xb1, 0x40, 0xb1, 0x80, 0x8c, 0xf1, 0x87, 0xa4, 0xf4, 0xdf,
     0xc0, 0x4b, 0x05, 0x35, 0x7c, 0x5d, 0x1c, 0x0e, 0xea, 0xc4, 0xc6, 0x6f, 0x9f, 0xf7, 0xf2, 0xe6},
  },
  {
    // This is the test case from
    // http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf,
    // section F.3.15, CFB128-AES192
    24,
    {0x8e, 0x73, 0xb0, 0xf7, 0xda, 0x0e, 0x64, 0x52, 0xc8, 0x10, 0xf3, 0x2b, 0x80, 0x90, 0x79, 0xe5,
     0x62, 0xf8, 0xea, 0xd2, 0x52, 0x2c, 0x6b, 0x7b},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
    {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
     0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
     0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
     0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10},
    {0xcd, 0xc8, 0x0d, 0x6f, 0xdd, 0xf1, 0x8c, 0xab, 0x34, 0xc2, 0x59, 0x09, 0xc9, 0x9a, 0x41, 0x74,
     0x67, 0xce, 0x7f, 0x7f, 0x81, 0x17, 0x36, 0x21, 0x96, 0x1a, 0x2b, 0x70, 0x17, 0x1d, 0x3d, 0x7a,
     0x2e, 0x1e, 0x8a, 0x1d, 0xd5, 0x9b, 0x88, 0xb1, 0xc8, 0xe6, 0x0f, 0xed, 0x1e, 0xfa, 0xc4, 0xc9,
     0xc0, 0x5f, 0x9f, 0x9c, 0xa9, 0x83, 0x4f, 0xa0, 0x42, 0xae, 0x8f, 0xba, 0x58, 0x4b, 0x09, 0xff},
  },
  {
    // This is the test case from
    // http://nvlpubs.nist.gov/nistpubs/Legacy/SP/nistspecialpublication800-38a.pdf,
    // section F.3.17, CFB128-AES256
    32,
    {0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe, 0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
     0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7, 0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f},
    {0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96, 0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a,
     0xae, 0x2d, 0x8a, 0x57, 0x1e, 0x03, 0xac, 0x9c, 0x9e, 0xb7, 0x6f, 0xac, 0x45, 0xaf, 0x8e, 0x51,
     0x30, 0xc8, 0x1c, 0x46, 0xa3, 0x5c, 0xe4, 0x11, 0xe5, 0xfb, 0xc1, 0x19, 0x1a, 0x0a, 0x52, 0xef,
     0xf6, 0x9f, 0x24, 0x45, 0xdf, 0x4f, 0x9b, 0x17, 0xad, 0x2b, 0x41, 0x7b, 0xe6, 0x6c, 0x37, 0x10},
    {0xdc, 0x7e, 0x84, 0xbf, 0xda, 0x79, 0x16, 0x4b, 0x7e, 0xcd, 0x84, 0x86, 0x98, 0x5d, 0x38, 0x60,
     0x39, 0xff, 0xed, 0x14, 0x3b, 0x28, 0xb1, 0xc8, 0x32, 0x11, 0x3c, 0x63, 0x31, 0xe5, 0x40, 0x7b,
     0xdf, 0x10, 0x13, 0x24, 0x15, 0xe5, 0x4b, 0x92, 0xa1, 0x3e, 0xd0, 0xa8, 0x26, 0x7a, 0xe2, 0xf9,
     0x75, 0xa3, 0x85, 0x74, 0x1a, 0xb9, 0xce, 0xf8, 0x20, 0x31, 0x62, 0x3d, 0x55, 0xb1, 0xe4, 0x71},
  },
};

TEST(CFBTest, TestVectors) {
  unsigned test_num = 0;
  for (const auto &test : kCFBTestCases) {
    test_num++;
    SCOPED_TRACE(test_num);

    const size_t input_len = sizeof(test.plaintext);
    std::unique_ptr<uint8_t[]> out(new uint8_t[input_len]);

    for (size_t stride = 1; stride <= input_len; stride++) {
      bssl::ScopedEVP_CIPHER_CTX ctx;
      if (test.key_len == 16) {
        ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), EVP_aes_128_cfb128(), nullptr,
                                       test.key, test.iv));
      } else if (test.key_len == 24) {
        ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), EVP_aes_192_cfb128(), nullptr,
                                       test.key, test.iv));
      } else {
        assert(test.key_len == 32);
        ASSERT_TRUE(EVP_EncryptInit_ex(ctx.get(), EVP_aes_256_cfb128(), nullptr,
                                       test.key, test.iv));
      }

      size_t done = 0;
      while (done < input_len) {
        size_t todo = stride;
        if (todo > input_len - done) {
          todo = input_len - done;
        }

        int out_bytes;
        ASSERT_TRUE(EVP_EncryptUpdate(ctx.get(), out.get() + done, &out_bytes,
                                      test.plaintext + done, todo));
        ASSERT_EQ(static_cast<size_t>(out_bytes), todo);

        done += todo;
      }

      EXPECT_EQ(Bytes(test.ciphertext), Bytes(out.get(), input_len));
    }

    bssl::ScopedEVP_CIPHER_CTX decrypt_ctx;
    if (test.key_len == 16) {
      ASSERT_TRUE(EVP_DecryptInit_ex(decrypt_ctx.get(), EVP_aes_128_cfb128(),
                                     nullptr, test.key, test.iv));
    } else if (test.key_len == 24) {
      ASSERT_TRUE(EVP_DecryptInit_ex(decrypt_ctx.get(), EVP_aes_192_cfb128(),
                                     nullptr, test.key, test.iv));
    } else {
      assert(test.key_len == 32);
      ASSERT_TRUE(EVP_DecryptInit_ex(decrypt_ctx.get(), EVP_aes_256_cfb128(),
                                     nullptr, test.key, test.iv));
    }

    std::unique_ptr<uint8_t[]> plaintext(new uint8_t[input_len]);
    int num_bytes;
    ASSERT_TRUE(EVP_DecryptUpdate(decrypt_ctx.get(), plaintext.get(),
                                  &num_bytes, out.get(), input_len));
    EXPECT_EQ(static_cast<size_t>(num_bytes), input_len);
    EXPECT_EQ(Bytes(test.plaintext), Bytes(plaintext.get(), input_len));
  }
}
