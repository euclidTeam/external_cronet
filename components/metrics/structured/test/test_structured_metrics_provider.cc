// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/structured/test/test_structured_metrics_provider.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::structured {

TestStructuredMetricsProvider::TestStructuredMetricsProvider() {
  if (temp_dir_.CreateUniqueTempDir()) {
    system_profile_provider_ = std::make_unique<MetricsProvider>();
    structured_metrics_recorder_ = std::unique_ptr<StructuredMetricsRecorder>(
        new StructuredMetricsRecorder(
            temp_dir_.GetPath()
                .Append(FILE_PATH_LITERAL("structured_metrics"))
                .Append(FILE_PATH_LITERAL("device_keys")),
            base::Seconds(0), system_profile_provider_.get()));
    structured_metrics_provider_ = std::unique_ptr<StructuredMetricsProvider>(
        new StructuredMetricsProvider(base::Seconds(0),
                                      structured_metrics_recorder_.get()));
    Recorder::GetInstance()->AddObserver(this);
  }
}

TestStructuredMetricsProvider::~TestStructuredMetricsProvider() {
  Recorder::GetInstance()->RemoveObserver(this);
}

void TestStructuredMetricsProvider::EnableRecording() {
  structured_metrics_provider_->OnRecordingEnabled();
}

void TestStructuredMetricsProvider::DisableRecording() {
  structured_metrics_provider_->OnRecordingDisabled();
}

const EventsProto& TestStructuredMetricsProvider::ReadEvents() {
  return *structured_metrics_provider_->recorder().events();
}

absl::optional<const StructuredEventProto*>
TestStructuredMetricsProvider::FindEvent(uint64_t project_name_hash,
                                         uint64_t event_name_hash) {
  const EventsProto& events = TestStructuredMetricsProvider::ReadEvents();
  for (const auto& event : events.non_uma_events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      return &event;
    }
  }
  return absl::nullopt;
}

std::vector<const StructuredEventProto*>
TestStructuredMetricsProvider::FindEvents(uint64_t project_name_hash,
                                          uint64_t event_name_hash) {
  std::vector<const StructuredEventProto*> events_vector;
  const EventsProto& events = TestStructuredMetricsProvider::ReadEvents();
  for (const auto& event : events.non_uma_events()) {
    if (event.project_name_hash() == project_name_hash &&
        event.event_name_hash() == event_name_hash) {
      events_vector.push_back(&event);
    }
  }
  return events_vector;
}

void TestStructuredMetricsProvider::SetOnEventsRecordClosure(
    base::RepeatingCallback<void(const Event& event)> event_record_callback) {
  event_record_callback_ = std::move(event_record_callback);
}

void TestStructuredMetricsProvider::OnProfileAdded(
    const base::FilePath& profile_path) {
  structured_metrics_provider_->recorder().OnProfileAdded(profile_path);
}

void TestStructuredMetricsProvider::OnEventRecord(const Event& event) {
  structured_metrics_provider_->recorder().OnEventRecord(event);
  if (!event_record_callback_) {
    return;
  }

  event_record_callback_.Run(event);
}

void TestStructuredMetricsProvider::OnReportingStateChanged(bool enabled) {
  structured_metrics_provider_->recorder().OnReportingStateChanged(enabled);
}

absl::optional<int> TestStructuredMetricsProvider::LastKeyRotation(
    uint64_t project_name_hash) {
  return structured_metrics_provider_->recorder().LastKeyRotation(
      project_name_hash);
}

}  // namespace metrics::structured
