// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr_asan_service.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <sanitizer/allocator_interface.h>
#include <sanitizer/asan_interface.h>
#include <stdarg.h>
#include <string.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/memory/raw_ptr_asan_bound_arg_tracker.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool/thread_group.h"
#include "base/threading/thread_local.h"

namespace base {

RawPtrAsanService RawPtrAsanService::instance_;

namespace {
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_mapping.h#L154
constexpr size_t kShadowScale = 3;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_allocator.cpp#L143
constexpr size_t kChunkHeaderSize = 16;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L138
constexpr uint8_t kAsanHeapLeftRedzoneMagic = 0xfa;
// https://github.com/llvm/llvm-project/blob/b84673b3f424882c4c1961fb2c49b6302b68f344/compiler-rt/lib/asan/asan_internal.h#L145
constexpr uint8_t kAsanUserPoisonedMemoryMagic = 0xf7;
}  // namespace

#if defined(COMPONENT_BUILD) && defined(_WIN32)
// In component builds on Windows, weak function exported by ASan have the
// `__dll` suffix. ASan itself uses the `alternatename` directive to account for
// that.
#pragma comment(linker, "/alternatename:__sanitizer_report_error_summary="     \
                        "__sanitizer_report_error_summary__dll")
#endif  // defined(COMPONENT_BUILD) && defined(_WIN32)

// static
void RawPtrAsanService::Log(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  auto formatted_message = StringPrintV(format, ap);
  va_end(ap);

  // Despite its name, the function just prints the input to the destination
  // configured by ASan.
  __sanitizer_report_error_summary(formatted_message.c_str());
}

// Mark the first eight bytes of every allocation's header as "user poisoned".
// This allows us to filter out allocations made before BRP-ASan is activated.
// The change shouldn't reduce the regular ASan coverage.

// static
NO_SANITIZE("address")
void RawPtrAsanService::MallocHook(const volatile void* ptr, size_t size) {
  uint8_t* header =
      static_cast<uint8_t*>(const_cast<void*>(ptr)) - kChunkHeaderSize;
  *RawPtrAsanService::GetInstance().GetShadow(header) =
      kAsanUserPoisonedMemoryMagic;
}

NO_SANITIZE("address")
bool RawPtrAsanService::IsSupportedAllocation(void* allocation_start) const {
  uint8_t* header = static_cast<uint8_t*>(allocation_start) - kChunkHeaderSize;
  return *GetShadow(header) == kAsanUserPoisonedMemoryMagic;
}

NO_SANITIZE("address")
void RawPtrAsanService::Configure(
    EnableDereferenceCheck enable_dereference_check,
    EnableExtractionCheck enable_extraction_check,
    EnableInstantiationCheck enable_instantiation_check) {
  CHECK_EQ(mode_, Mode::kUninitialized);

  Mode new_mode = enable_dereference_check || enable_extraction_check ||
                          enable_instantiation_check
                      ? Mode::kEnabled
                      : Mode::kDisabled;
  if (new_mode == Mode::kEnabled) {
    // The constants we use aren't directly exposed by the API, so
    // validate them at runtime as carefully as possible.
    size_t shadow_scale;
    __asan_get_shadow_mapping(&shadow_scale, &shadow_offset_);
    CHECK_EQ(shadow_scale, kShadowScale);

    uint8_t* dummy_alloc = new uint8_t;
    CHECK_EQ(*GetShadow(dummy_alloc - kChunkHeaderSize),
             kAsanHeapLeftRedzoneMagic);

    __asan_poison_memory_region(dummy_alloc, 1);
    CHECK_EQ(*GetShadow(dummy_alloc), kAsanUserPoisonedMemoryMagic);
    delete dummy_alloc;

    __sanitizer_install_malloc_and_free_hooks(MallocHook, FreeHook);
    __asan_set_error_report_callback(ErrorReportCallback);

    is_dereference_check_enabled_ = !!enable_dereference_check;
    is_extraction_check_enabled_ = !!enable_extraction_check;
    is_instantiation_check_enabled_ = !!enable_instantiation_check;
  }

  mode_ = new_mode;
}

uint8_t* RawPtrAsanService::GetShadow(void* ptr) const {
  return reinterpret_cast<uint8_t*>(
      (reinterpret_cast<uintptr_t>(ptr) >> kShadowScale) + shadow_offset_);
}

// static
void RawPtrAsanService::SetPendingReport(ReportType type,
                                         const volatile void* ptr) {
  // The actual ASan crash may occur at an offset from the pointer passed
  // here, so track the whole region.
  void* region_base;
  size_t region_size;
  __asan_locate_address(const_cast<void*>(ptr), nullptr, 0, &region_base,
                        &region_size);

  GetPendingReport() = {type, reinterpret_cast<uintptr_t>(region_base),
                        region_size};
}

namespace {
enum class ProtectionStatus {
  kNotProtected,
  kManualAnalysisRequired,
  kProtected,
};

const char* ProtectionStatusToString(ProtectionStatus status) {
  switch (status) {
    case ProtectionStatus::kNotProtected:
      return "NOT PROTECTED";
    case ProtectionStatus::kManualAnalysisRequired:
      return "MANUAL ANALYSIS REQUIRED";
    case ProtectionStatus::kProtected:
      return "PROTECTED";
  }
}

// ASan doesn't have an API to get the current thread's identifier.
// We have to create a dummy allocation to determine it.
int GetCurrentThreadId() {
  int* dummy = new int;
  int id = -1;
  __asan_get_alloc_stack(dummy, nullptr, 0, &id);
  delete dummy;
  return id;
}
}  // namespace

// static
void RawPtrAsanService::ErrorReportCallback(const char* report) {
  if (strcmp(__asan_get_report_description(), "heap-use-after-free") != 0)
    return;

  struct {
    ProtectionStatus protection_status;
    const char* crash_details;
    const char* protection_details;
  } crash_info;

  auto& pending_report = GetPendingReport();
  uintptr_t ptr = reinterpret_cast<uintptr_t>(__asan_get_report_address());
  uintptr_t bound_arg_ptr = RawPtrAsanBoundArgTracker::GetProtectedArgPtr(ptr);
  if (pending_report.allocation_base <= ptr &&
      ptr < pending_report.allocation_base + pending_report.allocation_size) {
    bool is_supported_allocation =
        RawPtrAsanService::GetInstance().IsSupportedAllocation(
            reinterpret_cast<void*>(pending_report.allocation_base));
    switch (pending_report.type) {
      case ReportType::kDereference: {
        if (is_supported_allocation) {
          crash_info = {ProtectionStatus::kProtected,
                        "This crash occurred while a raw_ptr<T> object "
                        "containing a dangling pointer was being dereferenced.",
                        "MiraclePtr is expected to make this crash "
                        "non-exploitable once fully enabled."};
        } else {
          crash_info = {ProtectionStatus::kNotProtected,
                        "This crash occurred while accessing a region that was "
                        "allocated before MiraclePtr was activated.",
                        "This crash is still exploitable with MiraclePtr."};
        }
        break;
      }
      case ReportType::kExtraction: {
        if (is_supported_allocation && bound_arg_ptr) {
          crash_info = {
              ProtectionStatus::kProtected,
              "This crash occurred inside a callback where a raw_ptr<T> "
              "pointing to the same region was bound to one of the arguments.",
              "MiraclePtr is expected to make this crash non-exploitable once "
              "fully enabled."};
        } else if (is_supported_allocation) {
          crash_info = {
              ProtectionStatus::kManualAnalysisRequired,
              "A pointer to the same region was extracted from a raw_ptr<T> "
              "object prior to this crash.",
              "To determine the protection status, enable extraction warnings "
              "and check whether the raw_ptr<T> object can be destroyed or "
              "overwritten between the extraction and use."};
        } else {
          crash_info = {ProtectionStatus::kNotProtected,
                        "This crash occurred while accessing a region that was "
                        "allocated before MiraclePtr was activated.",
                        "This crash is still exploitable with MiraclePtr."};
        }
        break;
      }
      case ReportType::kInstantiation: {
        crash_info = {ProtectionStatus::kNotProtected,
                      "A pointer to an already freed region was assigned to a "
                      "raw_ptr<T> object, which may lead to memory corruption.",
                      "This crash is still exploitable with MiraclePtr."};
      }
    }
  } else if (bound_arg_ptr) {
    // Note - this branch comes second to avoid hiding invalid instantiations,
    // as we still consider it to be an error to instantiate a raw_ptr<T> from
    // an invalid T* even if that T* is guaranteed to be quarantined.
    bool is_supported_allocation =
        RawPtrAsanService::GetInstance().IsSupportedAllocation(
            reinterpret_cast<void*>(bound_arg_ptr));
    if (is_supported_allocation) {
      crash_info = {
          ProtectionStatus::kProtected,
          "This crash occurred inside a callback where a raw_ptr<T> pointing "
          "to the same region was bound to one of the arguments.",
          "MiraclePtr is expected to make this crash non-exploitable once "
          "fully enabled."};
    } else {
      crash_info = {ProtectionStatus::kNotProtected,
                    "This crash occurred while accessing a region that was "
                    "allocated before MiraclePtr was activated.",
                    "This crash is still exploitable with MiraclePtr."};
    }
  } else {
    crash_info = {
        ProtectionStatus::kNotProtected,
        "No raw_ptr<T> access to this region was detected prior to this crash.",
        "This crash is still exploitable with MiraclePtr."};
  }

  // The race condition check below may override the protection status.
  if (crash_info.protection_status != ProtectionStatus::kNotProtected) {
    int free_thread_id = -1;
    __asan_get_free_stack(reinterpret_cast<void*>(ptr), nullptr, 0,
                          &free_thread_id);
    if (free_thread_id != GetCurrentThreadId()) {
      crash_info.protection_status = ProtectionStatus::kManualAnalysisRequired;
      crash_info.protection_details =
          "The \"use\" and \"free\" threads don't match. This crash is likely "
          "to have been caused by a race condition that is mislabeled as a "
          "use-after-free. Make sure that the \"free\" is sequenced after the "
          "\"use\" (e.g. both are on the same sequence, or the \"free\" is in "
          "a task posted after the \"use\"). Otherwise, the crash is still "
          "exploitable with MiraclePtr.";
    } else if (internal::ThreadGroup::CurrentThreadHasGroup()) {
      // We need to be especially careful with ThreadPool threads. Otherwise,
      // we might miss false-positives where the "use" and "free" happen on
      // different sequences but the same thread by chance.
      crash_info.protection_status = ProtectionStatus::kManualAnalysisRequired;
      crash_info.protection_details =
          "This crash occurred in the thread pool. The sequence which invoked "
          "the \"free\" is unknown, so the crash may have been caused by a "
          "race condition that is mislabeled as a use-after-free. Make sure "
          "that the \"free\" is sequenced after the \"use\" (e.g. both are on "
          "the same sequence, or the \"free\" is in a task posted after the "
          "\"use\"). Otherwise, the crash is still exploitable with "
          "MiraclePtr.";
    }
  }

  Log("\nMiraclePtr Status: %s\n%s\n%s\n"
      "Refer to "
      "https://chromium.googlesource.com/chromium/src/+/main/base/memory/"
      "raw_ptr.md for details.",
      ProtectionStatusToString(crash_info.protection_status),
      crash_info.crash_details, crash_info.protection_details);
}

// static
RawPtrAsanService::PendingReport& RawPtrAsanService::GetPendingReport() {
  // Intentionally use thread-local-storage here. Making this sequence-local
  // doesn't prevent sharing of PendingReport contents between unrelated
  // tasks, so we keep this at a lower-level and avoid introducing additional
  // assumptions about Chrome's sequence model.
  static NoDestructor<ThreadLocalOwnedPointer<PendingReport>> pending_report;
  PendingReport* raw_pending_report = pending_report->Get();
  if (UNLIKELY(!raw_pending_report)) {
    auto new_pending_report = std::make_unique<PendingReport>();
    raw_pending_report = new_pending_report.get();
    pending_report->Set(std::move(new_pending_report));
  }
  return *raw_pending_report;
}

}  // namespace base
#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
