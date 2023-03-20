// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_H_
#define BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_H_

#include <stddef.h>
#include <stdint.h>

#include <functional>

#include "base/base_export.h"

namespace base {
namespace trace_event {

// When heap profiling is enabled, tracing keeps track of the allocation
// context for each allocation intercepted. It is generated by the
// |AllocationContextTracker| which keeps stacks of context in TLS.
// The tracker is initialized lazily.

// The backtrace in the allocation context is a snapshot of the native call
// stack.

// Represents a stack frame. Used in Backtrace class below.
//
// Conceptually stack frame is identified by its value, and type is used
// mostly to properly format the value. Value is expected to be a valid
// pointer from process' address space.
struct BASE_EXPORT StackFrame {
  enum class Type {
    THREAD_NAME,        // const char* thread name
    PROGRAM_COUNTER,    // as returned by stack tracing (e.g. by StackTrace)
  };

  static StackFrame FromThreadName(const char* name) {
    return {Type::THREAD_NAME, name};
  }
  static StackFrame FromProgramCounter(const void* pc) {
    return {Type::PROGRAM_COUNTER, pc};
  }

  Type type;
  const void* value;
};

bool BASE_EXPORT operator < (const StackFrame& lhs, const StackFrame& rhs);
bool BASE_EXPORT operator == (const StackFrame& lhs, const StackFrame& rhs);
bool BASE_EXPORT operator != (const StackFrame& lhs, const StackFrame& rhs);

struct BASE_EXPORT Backtrace {
  Backtrace();

  // If the stack is higher than what can be stored here, the top frames
  // (the ones further from main()) are stored. Depth of 12 is enough for most
  // pseudo traces (see above), but not for native traces, where we need more.
  enum { kMaxFrameCount = 48 };
  StackFrame frames[kMaxFrameCount];
  size_t frame_count = 0;
};

bool BASE_EXPORT operator==(const Backtrace& lhs, const Backtrace& rhs);
bool BASE_EXPORT operator!=(const Backtrace& lhs, const Backtrace& rhs);

// The |AllocationContext| is context metadata that is kept for every allocation
// when heap profiling is enabled. To simplify memory management for book-
// keeping, this struct has a fixed size.
struct BASE_EXPORT AllocationContext {
  AllocationContext();
  AllocationContext(const Backtrace& backtrace, const char* type_name);

  Backtrace backtrace;

  // Type name of the type stored in the allocated memory. A null pointer
  // indicates "unknown type". Grouping is done by comparing pointers, not by
  // deep string comparison. In a component build, where a type name can have a
  // string literal in several dynamic libraries, this may distort grouping.
  const char* type_name;
};

bool BASE_EXPORT operator==(const AllocationContext& lhs,
                            const AllocationContext& rhs);
bool BASE_EXPORT operator!=(const AllocationContext& lhs,
                            const AllocationContext& rhs);

// Struct to store the size and count of the allocations.
struct AllocationMetrics {
  size_t size;
  size_t count;
};

}  // namespace trace_event
}  // namespace base

namespace std {

template <>
struct BASE_EXPORT hash<base::trace_event::StackFrame> {
  size_t operator()(const base::trace_event::StackFrame& frame) const;
};

template <>
struct BASE_EXPORT hash<base::trace_event::Backtrace> {
  size_t operator()(const base::trace_event::Backtrace& backtrace) const;
};

template <>
struct BASE_EXPORT hash<base::trace_event::AllocationContext> {
  size_t operator()(const base::trace_event::AllocationContext& context) const;
};

}  // namespace std

#endif  // BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_H_
