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
template <bool checked>
class JNI_ZERO_COMPONENT_BUILD_EXPORT JniJavaCallContext {
 public:
  JNI_ZERO_ALWAYS_INLINE JniJavaCallContext() {
// TODO(ssid): Implement for other architectures.
#if defined(__arm__) || defined(__aarch64__)
    // This assumes that this method does not increment the stack pointer.
    asm volatile("mov %0, sp" : "=r"(sp_));
#else
    sp_ = 0;
#endif
  }

  // Force no inline to reduce code size.
  template <MethodID::Type type>
  JNI_ZERO_NOINLINE void Init(JNIEnv* env,
                              jclass clazz,
                              const char* method_name,
                              const char* jni_signature,
                              std::atomic<jmethodID>* atomic_method_id) {
    env_ = env;

    // Make sure compiler doesn't optimize out the assignment.
    memcpy(&marker_, &kJniStackMarkerValue, sizeof(kJniStackMarkerValue));
    // Gets PC of the calling function.
    pc_ = reinterpret_cast<uintptr_t>(__builtin_return_address(0));

    method_id_ = MethodID::LazyGet<type>(env, clazz, method_name, jni_signature,
                                         atomic_method_id);
  }

  JNI_ZERO_NOINLINE ~JniJavaCallContext() {
    // Reset so that spurious marker finds are avoided.
    memset(&marker_, 0, sizeof(marker_));
    if (checked) {
      CheckException(env_);
    }
  }

  jmethodID method_id() { return method_id_; }

 private:
  uint64_t marker_;
  uintptr_t sp_;
  uintptr_t pc_;
  JNIEnv* env_;
  jmethodID method_id_;
};

}  // namespace jni_zero::internal

#endif  // JNI_ZERO_JNI_ZERO_INTERNAL_H
