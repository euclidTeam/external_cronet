// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_NAMED_TRIGGER_H_
#define BASE_TRACE_EVENT_NAMED_TRIGGER_H_

#include <string>

#include "base/base_export.h"

namespace base::trace_event {

// Notifies that a manual trigger event has occurred. Returns true if the
// trigger caused a scenario to either begin recording or finalize the trace
// depending on the config, or false if the trigger had no effect. If the
// trigger specified isn't active in the config, this will do nothing.
BASE_EXPORT bool EmitNamedTrigger(const std::string& trigger_name);

class NamedTriggerManager {
 public:
  virtual bool DoEmitNamedTrigger(const std::string& trigger_name) = 0;

 protected:
  // Sets the instance returns by GetInstance() globally to |manager|.
  BASE_EXPORT static void SetInstance(NamedTriggerManager* manager);
};

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_NAMED_TRIGGER_H_
