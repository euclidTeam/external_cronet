// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_trace_processor.h"
#include "base/trace_event/trace_log.h"

namespace base::test {

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

TraceConfig DefaultTraceConfig(const StringPiece& category_filter_string,
                               bool privacy_filtering) {
  TraceConfig trace_config;
  auto* buffer_config = trace_config.add_buffers();
  buffer_config->set_size_kb(4 * 1024);

  auto* data_source = trace_config.add_data_sources();
  auto* source_config = data_source->mutable_config();
  source_config->set_name("track_event");
  source_config->set_target_buffer(0);

  perfetto::protos::gen::TrackEventConfig track_event_config;
  base::trace_event::TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(category_filter_string);

  // If no categories are explicitly enabled, enable the default ones.
  // Otherwise only matching categories are enabled.
  if (!category_filter.included_categories().empty()) {
    track_event_config.add_disabled_categories("*");
  }
  for (const auto& included_category : category_filter.included_categories()) {
    track_event_config.add_enabled_categories(included_category);
  }
  for (const auto& disabled_category : category_filter.disabled_categories()) {
    track_event_config.add_enabled_categories(disabled_category);
  }
  for (const auto& excluded_category : category_filter.excluded_categories()) {
    track_event_config.add_disabled_categories(excluded_category);
  }

  source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());

  if (privacy_filtering) {
    track_event_config.set_filter_debug_annotations(true);
    track_event_config.set_filter_dynamic_event_names(true);
  }

  return trace_config;
}

TestTraceProcessor::TestTraceProcessor() = default;
TestTraceProcessor::~TestTraceProcessor() = default;

void TestTraceProcessor::StartTrace(const StringPiece& category_filter_string,
                                    bool privacy_filtering) {
  StartTrace(DefaultTraceConfig(category_filter_string, privacy_filtering));
}

void TestTraceProcessor::StartTrace(const TraceConfig& config,
                                    perfetto::BackendType backend) {
  // Try to guess the correct backend if it's unspecified. In unit tests
  // Perfetto is initialized by TraceLog, and only the in-process backend is
  // available. In browser tests multiple backend can be available, so we
  // explicitly specialize the custom backend to prevent tests from connecting
  // to a system backend.
  if (backend == perfetto::kUnspecifiedBackend) {
    if (base::trace_event::TraceLog::GetInstance()
            ->IsPerfettoInitializedByTraceLog()) {
      backend = perfetto::kInProcessBackend;
    } else {
      backend = perfetto::kCustomBackend;
    }
  }
  session_ = perfetto::Tracing::NewTrace(backend);
  session_->Setup(config);
  // Some tests run the tracing service on the main thread and StartBlocking()
  // can deadlock so use a RunLoop instead.
  base::RunLoop run_loop;
  session_->SetOnStartCallback([&run_loop]() { run_loop.QuitWhenIdle(); });
  session_->Start();
  run_loop.Run();
}

absl::Status TestTraceProcessor::StopAndParseTrace() {
  base::TrackEvent::Flush();
  session_->StopBlocking();
  std::vector<char> trace = session_->ReadTraceBlocking();
  return test_trace_processor_.ParseTrace(trace);
}

base::expected<TestTraceProcessor::QueryResult, std::string>
TestTraceProcessor::RunQuery(const std::string& query) {
  auto result_or_error = test_trace_processor_.ExecuteQuery(query);
  if (!result_or_error.ok()) {
    return base::unexpected(result_or_error.error());
  }
  return base::ok(result_or_error.result());
}

#endif  // BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

}  // namespace base::test
