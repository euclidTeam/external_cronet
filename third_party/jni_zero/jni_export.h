// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_JNI_EXPORT_H_
#define JNI_ZERO_JNI_EXPORT_H_

#if defined(COMPONENT_BUILD)
#define JNI_ZERO_COMPONENT_BUILD_EXPORT __attribute__((visibility("default")))
#else
#define JNI_ZERO_COMPONENT_BUILD_EXPORT
#endif

#endif  // JNI_ZERO_JNI_EXPORT_H_
