#pragma once

#include <datadog/optional.h>
#include <datadog/span_config.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include "developer_noise.h"
#include "httplib.h"
#include "manual_scheduler.h"
#include "utils.h"

class RequestHandler final {
 public:
  RequestHandler(datadog::tracing::FinalizedTracerConfig& tracerConfig,
                 std::shared_ptr<ManualScheduler> scheduler,
                 std::shared_ptr<DeveloperNoiseLogger> logger);

  void set_error(const char* const file, int line, const std::string& reason,
                 httplib::Response& res);

  void on_span_start(const httplib::Request& req, httplib::Response& res);
  void on_span_end(const httplib::Request& req, httplib::Response& res);
  void on_set_meta(const httplib::Request& req, httplib::Response& res);
  void on_set_metric(const httplib::Request& /* req */, httplib::Response& res);
  void on_inject_headers(const httplib::Request& req, httplib::Response& res);
  void on_span_flush(const httplib::Request& /* req */, httplib::Response& res);
  void on_stats_flush(const httplib::Request& /* req */,
                      httplib::Response& res);
  void on_span_error(const httplib::Request& req, httplib::Response& res);

 private:
  datadog::tracing::Tracer tracer_;
  std::shared_ptr<ManualScheduler> scheduler_;
  std::shared_ptr<DeveloperNoiseLogger> logger_;
  std::unordered_map<uint64_t, datadog::tracing::Span> active_spans_;

#undef VALIDATION_ERROR
};
