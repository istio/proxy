#pragma once

#include <datadog/optional.h>
#include <datadog/span_config.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <datadog/json.hpp>

#include "developer_noise.h"
#include "httplib.h"
#include "manual_scheduler.h"

class RequestHandler final {
 public:
  RequestHandler(datadog::tracing::FinalizedTracerConfig& tracerConfig,
                 std::shared_ptr<ManualScheduler> scheduler,
                 std::shared_ptr<DeveloperNoiseLogger> logger);

  void set_error(const char* const file, int line, const std::string& reason,
                 httplib::Response& res);

  void on_trace_config(const httplib::Request& req, httplib::Response& res);
  void on_span_start(const httplib::Request& req, httplib::Response& res);
  void on_span_end(const httplib::Request& req, httplib::Response& res);
  void on_set_meta(const httplib::Request& req, httplib::Response& res);
  void on_set_metric(const httplib::Request& /* req */, httplib::Response& res);
  void on_inject_headers(const httplib::Request& req, httplib::Response& res);
  void on_extract_headers(const httplib::Request& req, httplib::Response& res);
  void on_span_flush(const httplib::Request& /* req */, httplib::Response& res);
  void on_stats_flush(const httplib::Request& /* req */,
                      httplib::Response& res);
  void on_span_error(const httplib::Request& req, httplib::Response& res);

 private:
  datadog::tracing::Tracer tracer_;
  std::shared_ptr<ManualScheduler> scheduler_;
  std::shared_ptr<DeveloperNoiseLogger> logger_;
  std::unordered_map<uint64_t, datadog::tracing::Span> active_spans_;
  std::unordered_map<uint64_t, nlohmann::json::array_t> tracing_context_;

  // Previously, `/trace/span/start` was used to create new spans or create
  // child spans from the extracted tracing context.
  //
  // The logic has been split into two distinct endpoint, with the addition of
  // `extract_headers`. However, the public API does not expose a method to just
  // extract tracing context.
  //
  // For now, the workaround is to extract and create a span from tracing
  // context and keep the span alive until the process terminate, thus
  // explaining the name :)
  std::vector<datadog::tracing::Span> blackhole_;

#undef VALIDATION_ERROR
};
