// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_JNI_ZERO_INTERNAL_H
#define JNI_ZERO_JNI_ZERO_INTERNAL_H

#include <jni.h>

#include "third_party/jni_zero/jni_export.h"
#include "third_party/jni_zero/jni_zero.h"
#include "third_party/jni_zero/logging.h"

// Project-specific macros used by the header files generated by
// jni_generator.py. Different projects can then specify their own
// implementation for this file.
#define CHECK_NATIVE_PTR(env, jcaller, native_ptr, method_name, ...) \
  JNI_ZERO_DCHECK(native_ptr);

#define CHECK_CLAZZ(env, jcaller, clazz, ...) JNI_ZERO_DCHECK(clazz);

#if defined(__clang__) && __has_attribute(noinline)
#define JNI_ZERO_NOINLINE [[clang::noinline]]
#elif __has_attribute(noinline)
#define JNI_ZERO_NOINLINE __attribute__((noinline))
#endif

#if defined(__clang__) && defined(NDEBUG) && __has_attribute(always_inline)
#define JNI_ZERO_ALWAYS_INLINE [[clang::always_inline]] inline
#elif defined(NDEBUG) && __has_attribute(always_inline)
#define JNI_ZERO_ALWAYS_INLINE inline __attribute__((__always_inline__))
#else
#define JNI_ZERO_ALWAYS_INLINE inline
#endif

namespace jni_zero::internal {

inline void HandleRegistrationError(JNIEnv* env,
                                    jclass clazz,
                                    const char* filename) {
  JNI_ZERO_ELOG("RegisterNatives failed in %s", filename);
}

// A 32 bit number could be an address on stack. Random 64 bit marker on the
// stack is much less likely to be present on stack.
inline constexpr uint64_t kJniStackMarkerValue = 0xbdbdef1bebcade1b;

// The method will initialize |atomic_class_id| to contain a global ref to the
// class. And will return that ref on subsequent calls.
JNI_ZERO_COMPONENT_BUILD_EXPORT jclass
LazyGetClass(JNIEnv* env,
             const char* class_name,
             const char* split_name,
             std::atomic<jclass>* atomic_class_id);

JNI_ZERO_COMPONENT_BUILD_EXPORT jclass
LazyGetClass(JNIEnv* env,
             const char* class_name,
             std::atomic<jclass>* atomic_class_id);

// Context about the JNI call with exception checked to be stored in stack.
struct JNI_ZERO_COMPONENT_BUILD_EXPORT JniJavaCallContextUnchecked {
  JNI_ZERO_ALWAYS_INLINE JniJavaCallContextUnchecked() {
// TODO(ssid): Implement for other architectures.
#if defined(__arm__) || defined(__aarch64__)
    // This assumes that this method does not increment the stack pointer.
    asm volatile("mov %0, sp" : "=r"(sp));
#else
    sp = 0;
#endif
  }

  // Force no inline to reduce code size.
  template <MethodID::Type type>
  JNI_ZERO_NOINLINE void Init(JNIEnv* env,
                              jclass clazz,
                              const char* method_name,
                              const char* jni_signature,
                              std::atomic<jmethodID>* atomic_method_id) {
    env1 = env;

    // Make sure compiler doesn't optimize out the assignment.
    memcpy(&marker, &kJniStackMarkerValue, sizeof(kJniStackMarkerValue));
    // Gets PC of the calling function.
    pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));

    method_id = MethodID::LazyGet<type>(env, clazz, method_name, jni_signature,
                                        atomic_method_id);
  }

  JNI_ZERO_NOINLINE ~JniJavaCallContextUnchecked() {
    // Reset so that spurious marker finds are avoided.
    memset(&marker, 0, sizeof(marker));
  }

  uint64_t marker;
  uintptr_t sp;
  uintptr_t pc;

  JNIEnv* env1;
  jmethodID method_id;
};

// Context about the JNI call with exception unchecked to be stored in stack.
struct JNI_ZERO_COMPONENT_BUILD_EXPORT JniJavaCallContextChecked {
  // Force no inline to reduce code size.
  template <MethodID::Type type>
  JNI_ZERO_NOINLINE void Init(JNIEnv* env,
                              jclass clazz,
                              const char* method_name,
                              const char* jni_signature,
                              std::atomic<jmethodID>* atomic_method_id) {
    base.Init<type>(env, clazz, method_name, jni_signature, atomic_method_id);
    // Reset |pc| to correct caller.
    base.pc = reinterpret_cast<uintptr_t>(__builtin_return_address(0));
  }

  JNI_ZERO_NOINLINE ~JniJavaCallContextChecked() { CheckException(base.env1); }

  JniJavaCallContextUnchecked base;
};

static_assert(sizeof(JniJavaCallContextChecked) ==
                  sizeof(JniJavaCallContextUnchecked),
              "Stack unwinder cannot work with structs of different sizes.");

}  // namespace jni_zero::internal

#endif  // JNI_ZERO_JNI_ZERO_INTERNAL_H
