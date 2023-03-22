// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_
#define COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {

// Observes logs generated by a metrics collection system (UMA and UKM) and
// stores them in-memory. This class also provides a way to export the logs in a
// JSON format, which includes metadata, proto data, as well as the events
// describing the lifecycle of the logs.
class MetricsServiceObserver : public MetricsLogsEventManager::Observer {
 public:
  // Possible metrics service types.
  enum class MetricsServiceType {
    UMA,
    UKM,
  };

  // Represents a log and its data. Exposed for testing.
  struct Log {
    // Represents an event that occurred on the log. An optional message may
    // be associated with the event. For example, the event may be
    // |kLogTrimmed|, with |message| being "Log size too large".
    struct Event {
      Event();

      Event(const Event&);
      Event& operator=(const Event&);

      ~Event();

      // The type of event.
      MetricsLogsEventManager::LogEvent event;

      // The timestamp at which the event occurred. This is the number of
      // milliseconds since Epoch.
      double timestampMs;

      // An optional message associated with the event.
      absl::optional<std::string> message;
    };

    Log();

    Log(const Log&);
    Log& operator=(const Log&);

    ~Log();

    // The SHA1 hash of the log's data, used to uniquely identify it.
    std::string hash;

    // The time at which the log was closed. This is the number of seconds since
    // Epoch.
    std::string timestamp;

    // The log's compressed (gzipped) serialized protobuf.
    std::string data;

    // A list of the events that occurred throughout the log's lifetime.
    std::vector<Event> events;

    // The type of log (stability, ongoing, independent). This is only set if
    // this log is a UMA log.
    absl::optional<MetricsLog::LogType> type;
  };

  // |service_type| is the type of service this observer will be observing from.
  explicit MetricsServiceObserver(MetricsServiceType service_type);

  MetricsServiceObserver(const MetricsServiceObserver&) = delete;
  MetricsServiceObserver& operator=(const MetricsServiceObserver&) = delete;

  ~MetricsServiceObserver() override;

  // MetricsLogsEventManager::Observer:
  void OnLogCreated(base::StringPiece log_hash,
                    base::StringPiece log_data,
                    base::StringPiece log_timestamp) override;
  void OnLogEvent(MetricsLogsEventManager::LogEvent event,
                  base::StringPiece log_hash,
                  base::StringPiece message) override;
  void OnLogType(absl::optional<MetricsLog::LogType> log_type) override;

  // Exports |logs_| to a JSON string and writes it to |json_output|. If
  // |include_log_proto_data| is true, the protos of the logs will be included.
  // The format of the JSON object is as follows:
  //
  // {
  //   logType: string, // e.g. "UMA" or "UKM"
  //   logs: [
  //     {
  //       type?: string, // e.g. "Ongoing" (set only for UMA logs)
  //       hash: string,
  //       timestamp: string,
  //       data: string, // set if |include_log_proto_data| is true
  //       size: number,
  //       events: [
  //         {
  //           event: string, // e.g. "Trimmed"
  //           timestamp: number,
  //           message?: string
  //         },
  //         ...
  //       ]
  //     },
  //     ...
  //   ]
  // }
  //
  // The "hash" field is the hex representation of the log's hash. The
  // "data" field is a base64 encoding of the log's compressed (gzipped)
  // serialized protobuf. The "size" field is the size (in bytes) of the log.
  bool ExportLogsAsJson(bool include_log_proto_data, std::string* json_output);

  // Exports logs data (see ExportLogsAsJson() above) to the passed |path|. If
  // the file pointed by |path| does not exist, it will be created. If it
  // already exists, its contents will be overwritten.
  void ExportLogsToFile(const base::FilePath& path);

  // Registers a callback. This callback will be run every time this observer is
  // notified through OnLogCreated() or OnLogEvent(). When the returned
  // CallbackListSubscription is destroyed, the callback is automatically
  // de-registered.
  [[nodiscard]] base::CallbackListSubscription AddNotifiedCallback(
      base::RepeatingClosure callback);

  // Returns |logs_|.
  std::vector<std::unique_ptr<Log>>* logs_for_testing() { return &logs_; }

 private:
  // Returns the Log object from |logs_| with the given |log_hash| if one
  // exists. Returns nullptr otherwise.
  Log* GetLogFromHash(base::StringPiece log_hash);

  // The type of service this observer is observing. This has no impact on how
  // the logs are stored. This is only used when exporting the logs (see
  // ExportLogsAsJson() above) so that the type of logs is easily identifiable.
  const MetricsServiceType service_type_;

  // The list of logs that are being kept track of. It is a vector so that we
  // can keep the ordering of the logs as they are inserted.
  std::vector<std::unique_ptr<Log>> logs_;

  // An overlay on |logs_| that allows for a log to be located based on its
  // hash.
  base::flat_map<base::StringPiece, Log*> indexed_logs_;

  // Keeps track of the type of UMA logs (ongoing, stability, independent) that
  // are being created. This should only be set for UMA logs, since the concept
  // of log type only exists in UMA.
  absl::optional<MetricsLog::LogType> uma_log_type_;

  // List of callbacks to run whenever this observer is notified. Note that
  // OnLogType() will not trigger the callbacks.
  base::RepeatingClosureList notified_callbacks_;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_METRICS_SERVICE_OBSERVER_H_
