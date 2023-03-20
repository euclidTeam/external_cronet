// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_info.h"

#include <ostream>
#include <utility>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace net {

base::StringPiece TransportTypeToString(TransportType type) {
  switch (type) {
    case TransportType::kDirect:
      return "TransportType::kDirect";
    case TransportType::kProxied:
      return "TransportType::kProxied";
    case TransportType::kCached:
      return "TransportType::kCached";
    case TransportType::kCachedFromProxy:
      return "TransportType::kCachedFromProxy";
  }

  // We define this here instead of as a `default` clause above so as to force
  // a compiler error if a new value is added to the enum and this method is
  // not updated to reflect it.
  NOTREACHED();
  return "<invalid transport type>";
}

TransportInfo::TransportInfo() = default;

TransportInfo::TransportInfo(TransportType type_arg,
                             IPEndPoint endpoint_arg,
                             std::string accept_ch_frame_arg)
    : type(type_arg),
      endpoint(std::move(endpoint_arg)),
      accept_ch_frame(std::move(accept_ch_frame_arg)) {
  switch (type) {
    case TransportType::kCached:
    case TransportType::kCachedFromProxy:
      DCHECK_EQ(accept_ch_frame, "");
      break;
    case TransportType::kDirect:
    case TransportType::kProxied:
      // `accept_ch_frame` can be empty or not. We use an exhaustive switch
      // statement to force this check to account for changes in the definition
      // of `TransportType`.
      break;
  }
}

TransportInfo::TransportInfo(const TransportInfo&) = default;

TransportInfo::~TransportInfo() = default;

bool TransportInfo::operator==(const TransportInfo& other) const {
  return type == other.type && endpoint == other.endpoint &&
         accept_ch_frame == other.accept_ch_frame;
}

bool TransportInfo::operator!=(const TransportInfo& other) const {
  return !(*this == other);
}

std::string TransportInfo::ToString() const {
  return base::StrCat({
      "TransportInfo{ type = ",
      TransportTypeToString(type),
      ", endpoint = ",
      endpoint.ToString(),
      ", accept_ch_frame = ",
      accept_ch_frame,
      " }",
  });
}

std::ostream& operator<<(std::ostream& out, TransportType type) {
  return out << TransportTypeToString(type);
}

std::ostream& operator<<(std::ostream& out, const TransportInfo& info) {
  return out << info.ToString();
}

}  // namespace net
