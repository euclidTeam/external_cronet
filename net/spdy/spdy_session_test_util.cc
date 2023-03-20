// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_session_test_util.h"

#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/task/current_thread.h"

namespace net {

SpdySessionTestTaskObserver::SpdySessionTestTaskObserver(
    const std::string& file_name,
    const std::string& function_name)
    : file_name_(file_name), function_name_(function_name) {
  base::CurrentThread::Get()->AddTaskObserver(this);
}

SpdySessionTestTaskObserver::~SpdySessionTestTaskObserver() {
  base::CurrentThread::Get()->RemoveTaskObserver(this);
}

void SpdySessionTestTaskObserver::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void SpdySessionTestTaskObserver::DidProcessTask(
    const base::PendingTask& pending_task) {
  if (base::EndsWith(pending_task.posted_from.file_name(), file_name_,
                     base::CompareCase::SENSITIVE) &&
      base::EndsWith(pending_task.posted_from.function_name(), function_name_,
                     base::CompareCase::SENSITIVE)) {
    ++executed_count_;
  }
}

}  // namespace net
