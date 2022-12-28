// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_
#define BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {

// BatteryLevelProvider provides an interface for querying battery state.
// A platform specific implementation is obtained with
// BatteryLevelProvider::Create().
class BASE_EXPORT BatteryLevelProvider {
 public:
  // The possible units of data used for the battery level.
  enum class BatteryLevelUnit {
    // Milliwatt-hour. This is desired as it is more precise.
    kMWh,
    // Milliampere-hour. Used when the capacity in ampere-hour is available but
    // not the voltage to convert to milliwatt-hour. Prefer mWh if available.
    kMAh,
    // Relative occurs when Windows returns imprecise battery counters.
    kRelative,
  };

  // Represents an aggregated state of all the batteries on the system at a
  // certain point in time.
  struct BatteryState {
    // Number of batteries on the system.
    int battery_count = 0;

    // Whether the system is connected to an external source of power. Defaults
    // to `true` if `battery_count` is 0.
    bool is_external_power_connected = false;

    // Current battery capacity. nullopt if `battery_count` != 1.
    absl::optional<uint64_t> current_capacity;

    // Fully charged battery capacity. nullopt if `battery_count` != 1.
    absl::optional<uint64_t> full_charged_capacity;

    // The unit of the battery's charge. Usually kMWh (milliwatt-hour) but can
    // be relative on Windows. nullopt if `battery_count` != 1.
    absl::optional<BatteryLevelUnit> charge_unit;

    // The time at which the battery state capture took place.
    base::TimeTicks capture_time;
  };

  // Creates a platform specific BatteryLevelProvider able to retrieve battery
  // state.
  static std::unique_ptr<BatteryLevelProvider> Create();

  virtual ~BatteryLevelProvider() = default;

  BatteryLevelProvider(const BatteryLevelProvider& other) = delete;
  BatteryLevelProvider& operator=(const BatteryLevelProvider& other) = delete;

  // Queries the current battery state and forwards it to `callback` when ready
  // (forwards nullopt on retrieval error). `callback` will not be invoked if
  // the BatteryLevelProvider is destroyed.
  virtual void GetBatteryState(
      base::OnceCallback<void(const absl::optional<BatteryState>&)>
          callback) = 0;

 protected:
  BatteryLevelProvider() = default;

  struct BatteryDetails {
    // Whether the battery is connected to an external power source.
    bool is_external_power_connected;

    // The current battery capacity.
    uint64_t current_capacity;

    // The battery's fully charged capacity.
    uint64_t full_charged_capacity;

    // The battery's unit of charge.
    BatteryLevelUnit charge_unit;
  };

  // Constructs a `BatteryState` from a list of `BatteryDetails`. The list can
  // be empty if there are no batteries on the system.
  static BatteryState MakeBatteryState(
      const std::vector<BatteryDetails>& battery_details);
};

}  // namespace base

#endif  // BASE_POWER_MONITOR_BATTERY_LEVEL_PROVIDER_H_
