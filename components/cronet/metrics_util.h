// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_METRICS_UTIL_H_
#define COMPONENTS_CRONET_METRICS_UTIL_H_

#include <stdint.h>

#include "base/time/time.h"

namespace cronet {

namespace metrics_util {

constexpr int64_t kNullTime = -1;

// Converts timing metrics stored as TimeTicks into the format expected by the
// API layer: ms since Unix epoch, or kNullTime for null (if either |ticks| or
// |start_ticks| is null).
//
// By calculating time values using a base (|start_ticks|, |start_time|) pair,
// time values are normalized. This allows time deltas between pairs of events
// to be accurately computed, even if the system clock changed between those
// events, as long as times for both events were calculated using the same
// (|start_ticks|, |start_time|) pair.
//
// Args:
//
// ticks: The ticks value corresponding to the time of the event -- the returned
//        time corresponds to this event.
//
// start_ticks: The ticks measurement at some base time -- the ticks equivalent
//              of start_time. Should be smaller than ticks.
//
// start_time: Time measurement at some base time -- the time equivalent of
//             start_ticks. Must not be null.
int64_t ConvertTime(const base::TimeTicks& ticks,
                    const base::TimeTicks& start_ticks,
                    const base::Time& start_time);

}  // namespace metrics_util

}  // namespace cronet

#endif  // COMPONENTS_CRONET_METRICS_UTIL_H_
