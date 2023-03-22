// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef QUICHE_QUIC_CORE_QUIC_STREAM_PRIORITY_H_
#define QUICHE_QUIC_CORE_QUIC_STREAM_PRIORITY_H_

#include <cstdint>
#include <string>
#include <tuple>

#include "absl/strings/string_view.h"
#include "quiche/quic/platform/api/quic_export.h"

namespace quic {

// Class to hold urgency and incremental values defined by
// https://httpwg.org/specs/rfc9218.html.
struct QUICHE_EXPORT QuicStreamPriority {
  static constexpr int kMinimumUrgency = 0;
  static constexpr int kMaximumUrgency = 7;
  static constexpr int kDefaultUrgency = 3;
  static constexpr bool kDefaultIncremental = false;

  // Parameter names for Priority Field Value.
  static constexpr absl::string_view kUrgencyKey = "u";
  static constexpr absl::string_view kIncrementalKey = "i";

  uint8_t urgency = kDefaultUrgency;
  bool incremental = kDefaultIncremental;

  bool operator==(const QuicStreamPriority& other) const {
    return std::tie(urgency, incremental) ==
           std::tie(other.urgency, other.incremental);
  }

  bool operator!=(const QuicStreamPriority& other) const {
    return !operator==(other);
  }
};

// Serializes the Priority Field Value for a PRIORITY_UPDATE frame.
QUICHE_EXPORT std::string SerializePriorityFieldValue(
    QuicStreamPriority priority);

// Return type of ParsePriorityFieldValue().
struct QUICHE_EXPORT ParsePriorityFieldValueResult {
  bool success;
  QuicStreamPriority priority;
};

// Parses the Priority Field Value field of a PRIORITY_UPDATE frame.
QUICHE_EXPORT ParsePriorityFieldValueResult
ParsePriorityFieldValue(absl::string_view priority_field_value);

}  // namespace quic

#endif  // QUICHE_QUIC_CORE_QUIC_STREAM_PRIORITY_H_
