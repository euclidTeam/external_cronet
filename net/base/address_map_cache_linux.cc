// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_map_cache_linux.h"

#include <linux/rtnetlink.h>

#include "base/synchronization/lock.h"

namespace net {

AddressMapCacheLinux::AddressMapCacheLinux() = default;
AddressMapCacheLinux::~AddressMapCacheLinux() = default;

AddressMapOwnerLinux::AddressMap AddressMapCacheLinux::GetAddressMap() const {
  base::AutoLock autolock(lock_);
  return cached_address_map_;
}

std::unordered_set<int> AddressMapCacheLinux::GetOnlineLinks() const {
  base::AutoLock autolock(lock_);
  return cached_online_links_;
}

void AddressMapCacheLinux::ApplyDiffs(const AddressMapDiff& addr_diff,
                                      const OnlineLinksDiff& links_diff) {
  base::AutoLock autolock(lock_);
  for (const auto& [address, msg_opt] : addr_diff) {
    if (msg_opt.has_value()) {
      cached_address_map_[address] = msg_opt.value();
    } else {
      DCHECK(cached_address_map_.count(address));
      cached_address_map_.erase(address);
    }
  }

  for (const auto& [if_index, is_now_online] : links_diff) {
    if (is_now_online) {
      DCHECK_EQ(cached_online_links_.count(if_index), 0u);
      cached_online_links_.insert(if_index);
    } else {
      DCHECK_EQ(cached_online_links_.count(if_index), 1u);
      cached_online_links_.erase(if_index);
    }
  }
}

}  // namespace net
