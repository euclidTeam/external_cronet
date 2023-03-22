// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_op.h"

#include <string.h>

#include <cstdio>
#include <sstream>

#include "base/logging.h"

namespace logging {

char* CheckOpValueStr(int v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%d", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%u", v);
  return strdup(buf);
}

char* CheckOpValueStr(long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%ld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lu", v);
  return strdup(buf);
}

char* CheckOpValueStr(long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%lld", v);
  return strdup(buf);
}

char* CheckOpValueStr(unsigned long long v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%llu", v);
  return strdup(buf);
}

char* CheckOpValueStr(const void* v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%p", v);
  return strdup(buf);
}

char* CheckOpValueStr(std::nullptr_t v) {
  return strdup("nullptr");
}

char* CheckOpValueStr(const std::string& v) {
  return strdup(v.c_str());
}

char* CheckOpValueStr(double v) {
  char buf[50];
  snprintf(buf, sizeof(buf), "%.6lf", v);
  return strdup(buf);
}

char* StreamValToStr(const void* v,
                     void (*stream_func)(std::ostream&, const void*)) {
  std::stringstream ss;
  stream_func(ss, v);
  return strdup(ss.str().c_str());
}

LogMessage* CheckOpResult::CreateLogMessage(bool is_dcheck,
                                            const char* file,
                                            int line,
                                            const char* expr_str,
                                            char* v1_str,
                                            char* v2_str) {
  LogMessage* const log_message =
      new LogMessage(file, line, is_dcheck ? LOGGING_DCHECK : LOGGING_FATAL);
  log_message->stream() << "Check failed: " << expr_str << " (" << v1_str
                        << " vs. " << v2_str << ")";
  free(v1_str);
  free(v2_str);
  return log_message;
}

}  // namespace logging
