// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_GENERATOR_JNI_GENERATOR_HELPER_H_
#define BASE_ANDROID_JNI_GENERATOR_JNI_GENERATOR_HELPER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_int_wrapper.h"
#include "base/android/scoped_java_ref.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

// Project-specific macros used by the header files generated by
// jni_generator.py. Different projects can then specify their own
// implementation for this file.
#define CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
  DCHECK(native_ptr) << method_name;

#define CHECK_CLAZZ(env, jcaller, clazz, ...) DCHECK(clazz);

#if defined(ARCH_CPU_X86)
// Dalvik JIT generated code doesn't guarantee 16-byte stack alignment on
// x86 - use force_align_arg_pointer to realign the stack at the JNI
// boundary. crbug.com/655248
#define JNI_GENERATOR_EXPORT \
  extern "C" __attribute__((visibility("default"), force_align_arg_pointer))
#else
#define JNI_GENERATOR_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Used to export JNI registration functions.
#if defined(COMPONENT_BUILD)
#define JNI_REGISTRATION_EXPORT __attribute__((visibility("default")))
#else
#define JNI_REGISTRATION_EXPORT
#endif

namespace jni_generator {

inline void HandleRegistrationError(JNIEnv* env,
                                    jclass clazz,
                                    const char* filename) {
  LOG(ERROR) << "RegisterNatives failed in " << filename;
}

inline void CheckException(JNIEnv* env) {
  base::android::CheckException(env);
}

// A 32 bit number could be an address on stack. Random 64 bit marker on the
// stack is much less likely to be present on stack.
constexpr uint64_t kJniStackMarkerValue = 0xbdbdef1bebcade1b;

// Context about the JNI call with exception checked to be stored in stack.
struct BASE_EXPORT JniJavaCallContextUnchecked {
  ALWAYS_INLINE JniJavaCallContextUnchecked() {
// TODO(ssid): Implement for other architectures.
#if defined(__arm__) || defined(__aarch64__)
    // This assumes that this method does not increment the stack pointer.
    asm volatile("mov %0, sp" : "=r"(sp));
#else
    sp = 0;
#endif
  }

  // Force no inline to reduce code size.
  template <base::android::MethodID::Type type>
  NOINLINE void Init(JNIEnv* env,
                     jclass clazz,
                     const char* method_name,
                     const char* jni_signature,
                     std::atomic<jmethodID>* atomic_method_id) {
    env1 = env;

    // Make sure compiler doesn't optimize out the assignment.
    memcpy(&marker, &kJniStackMarkerValue, sizeof(kJniStackMarkerValue));
    // Gets PC of the calling function.
    pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));

    method_id = base::android::MethodID::LazyGet<type>(
        env, clazz, method_name, jni_signature, atomic_method_id);
  }

  NOINLINE ~JniJavaCallContextUnchecked() {
    // Reset so that spurious marker finds are avoided.
    memset(&marker, 0, sizeof(marker));
  }

  uint64_t marker;
  uintptr_t sp;
  uintptr_t pc;

  raw_ptr<JNIEnv> env1;
  jmethodID method_id;
};

// Context about the JNI call with exception unchecked to be stored in stack.
struct BASE_EXPORT JniJavaCallContextChecked {
  // Force no inline to reduce code size.
  template <base::android::MethodID::Type type>
  NOINLINE void Init(JNIEnv* env,
                     jclass clazz,
                     const char* method_name,
                     const char* jni_signature,
                     std::atomic<jmethodID>* atomic_method_id) {
    base.Init<type>(env, clazz, method_name, jni_signature, atomic_method_id);
    // Reset |pc| to correct caller.
    base.pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  }

  NOINLINE ~JniJavaCallContextChecked() {
    jni_generator::CheckException(base.env1);
  }

  JniJavaCallContextUnchecked base;
};

static_assert(sizeof(JniJavaCallContextChecked) ==
                  sizeof(JniJavaCallContextUnchecked),
              "Stack unwinder cannot work with structs of different sizes.");

}  // namespace jni_generator

#endif  // BASE_ANDROID_JNI_GENERATOR_JNI_GENERATOR_HELPER_H_
