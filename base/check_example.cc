// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is meant for analyzing the code generated by the CHECK
// macros in a small executable file that's easy to disassemble.

#include <ostream>

#include "base/check_op.h"
#include "base/compiler_specific.h"

// An official build shouldn't generate code to print out messages for
// the CHECK* macros, nor should it have the strings in the
// executable. It is also important that the CHECK() function collapse to the
// same implementation as RELEASE_ASSERT(), in particular on Windows x86.
// Historically, the stream eating caused additional unnecessary instructions.
// See https://crbug.com/672699.

#define BLINK_RELEASE_ASSERT_EQUIVALENT(assertion) \
  (UNLIKELY(!(assertion)) ? (IMMEDIATE_CRASH()) : (void)0)

void DoCheck(bool b) {
  CHECK(b) << "DoCheck " << b;
}

void DoBlinkReleaseAssert(bool b) {
  BLINK_RELEASE_ASSERT_EQUIVALENT(b);
}

void DoCheckEq(int x, int y) {
  CHECK_EQ(x, y);
}

int main(int argc, const char* argv[]) {
  DoCheck(argc > 1);
  DoCheckEq(argc, 1);
  DoBlinkReleaseAssert(argc > 1);
}
