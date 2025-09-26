#include "request_handler.h"

#include <datadog/optional.h>
#include <datadog/span_config.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <datadog/json.hpp>

#include "httplib.h"
#include "utils.h"

RequestHandler::RequestHandler(
    datadog::tracing::FinalizedTracerConfig& tracerConfig,
    std::shared_ptr<ManualScheduler> scheduler,
    std::shared_ptr<DeveloperNoiseLogger> logger)
    : tracer_(tracerConfig),
      scheduler_(scheduler),
      logger_(std::move(logger)) {}

void RequestHandler::set_error(const char* const file, int line,
                               const std::string& reason,
                               httplib::Response& res) {
  logger_->log_info(reason);

  // clang-format off
    const auto error = nlohmann::json{
        {"detail", {
          {"loc", nlohmann::json::array({file, line})},
          {"msg", reason},
          {"type", "Validation Error"}
        }}
    };
  // clang-format on

  res.status = 422;
  res.set_content(error.dump(), "application/json");
}

#define VALIDATION_ERROR(res, msg)         \
  set_error(__FILE__, __LINE__, msg, res); \
  return

void RequestHandler::on_span_start(const httplib::Request& req,
                                   httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(req.body);

  datadog::tracing::SpanConfig span_cfg;
  span_cfg.name = request_json.at("name");

  if (auto service =
          utils::get_if_exists<std::string_view>(request_json, "service")) {
    span_cfg.service = *service;
  }

  if (auto service_type =
          utils::get_if_exists<std::string_view>(request_json, "type")) {
    span_cfg.service_type = *service_type;
  }

  if (auto resource =
          utils::get_if_exists<std::string_view>(request_json, "resource")) {
    span_cfg.resource = *resource;
  }

  if (auto origin =
          utils::get_if_exists<std::string_view>(request_json, "origin")) {
    logger_->log_info(
        "[start_span] origin, but this can only be set via the "
        "'x-datadog-origin' header");
  }

  auto success = [](const datadog::tracing::Span& span,
                    httplib::Response& res) {
    // clang-format off
      const auto response_body = nlohmann::json{
        { "trace_id", span.trace_id().low },
        { "span_id", span.id() }
      };
    // clang-format on

    res.set_content(response_body.dump(), "application/json");
  };

  if (auto parent_id =
          utils::get_if_exists<uint64_t>(request_json, "parent_id")) {
    if (*parent_id != 0) {
      auto parent_span_it = active_spans_.find(*parent_id);
      if (parent_span_it == active_spans_.cend()) {
        const auto msg = "on_span_start: span not found for id " +
                         std::to_string(*parent_id);
        VALIDATION_ERROR(res, msg);
      }

      auto span = parent_span_it->second.create_child(span_cfg);
      success(span, res);
      active_spans_.emplace(span.id(), std::move(span));
      return;
    }
  }

  if (auto http_headers = utils::get_if_exists<nlohmann::json::array_t>(
          request_json, "http_headers")) {
    if (!http_headers->empty()) {
      auto maybe_span = tracer_.extract_or_create_span(
          utils::HeaderReader(*http_headers), span_cfg);
      if (auto error = maybe_span.if_error()) {
        logger_->log_error(
            error->with_prefix("could not extract span from http_headers: "));
      } else {
        success(*maybe_span, res);
        active_spans_.emplace(maybe_span->id(), std::move(*maybe_span));
        return;
      }
    }
  }

  auto span = tracer_.create_span(span_cfg);
  success(span, res);
  active_spans_.emplace(span.id(), std::move(span));
}

void RequestHandler::on_span_end(const httplib::Request& req,
                                 httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(req.body);

  auto span_id = utils::get_if_exists<uint64_t>(request_json, "span_id");
  if (!span_id) {
    VALIDATION_ERROR(res, "on_span_end: missing `span_id` field.");
  }

  auto span_it = active_spans_.find(*span_id);
  if (span_it == active_spans_.cend()) {
    const auto msg =
        "on_span_end: span not found for id " + std::to_string(*span_id);
    VALIDATION_ERROR(res, msg);
  }

  active_spans_.erase(span_it);
  res.status = 200;
}

void RequestHandler::on_set_meta(const httplib::Request& req,
                                 httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(req.body);

  auto span_id = utils::get_if_exists<uint64_t>(request_json, "span_id");
  if (!span_id) {
    VALIDATION_ERROR(res, "on_set_meta: missing `span_id` field.");
  }

  auto span_it = active_spans_.find(*span_id);
  if (span_it == active_spans_.cend()) {
    const auto msg =
        "on_set_meta: span not found for id " + std::to_string(*span_id);
    VALIDATION_ERROR(res, msg);
  }

  auto& span = span_it->second;
  span.set_tag(request_json.at("key").get<std::string_view>(),
               request_json.at("value").get<std::string_view>());

  res.status = 200;
}

void RequestHandler::on_set_metric(const httplib::Request& /* req */,
                                   httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(res.body);

  auto span_id = utils::get_if_exists<uint64_t>(request_json, "span_id");
  if (!span_id) {
    VALIDATION_ERROR(res, "on_set_meta: missing `span_id` field.");
  }

  auto span_it = active_spans_.find(*span_id);
  if (span_it == active_spans_.cend()) {
    const auto msg =
        "on_set_meta: span not found for id " + std::to_string(*span_id);
    VALIDATION_ERROR(res, msg);
  }

  auto& span = span_it->second;
  span.set_metric(request_json.at("key").get<std::string_view>(),
                  request_json.at("value").get<double>());

  res.status = 200;
}

void RequestHandler::on_inject_headers(const httplib::Request& req,
                                       httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(req.body);

  auto span_id = utils::get_if_exists<uint64_t>(request_json, "span_id");
  if (!span_id) {
    VALIDATION_ERROR(res, "on_inject_headers: missing `span_id` field.");
  }

  auto span_it = active_spans_.find(*span_id);
  if (span_it == active_spans_.cend()) {
    const auto msg =
        "on_inject_headers: span not found for id " + std::to_string(*span_id);
    VALIDATION_ERROR(res, msg);
  }

  // clang-format off
    nlohmann::json response_json{
      { "http_headers", nlohmann::json::array() }
    };
  // clang-format on

  utils::HeaderWriter writer(response_json["http_headers"]);
  span_it->second.inject(writer);

  res.set_content(response_json.dump(), "application/json");
}

void RequestHandler::on_span_flush(const httplib::Request& /* req */,
                                   httplib::Response& res) {
  scheduler_->flush_telemetry();
  res.status = 200;
}

void RequestHandler::on_stats_flush(const httplib::Request& /* req */,
                                    httplib::Response& res) {
  scheduler_->flush_traces();
  res.status = 200;
}

void RequestHandler::on_span_error(const httplib::Request& req,
                                   httplib::Response& res) {
  const auto request_json = nlohmann::json::parse(req.body);

  auto span_id = utils::get_if_exists<uint64_t>(request_json, "span_id");
  if (!span_id) {
    VALIDATION_ERROR(res, "on_span_error: missing `span_id` field.");
  }

  auto span_it = active_spans_.find(*span_id);
  if (span_it == active_spans_.cend()) {
    const auto msg =
        "on_span_error: span not found for id " + std::to_string(*span_id);
    VALIDATION_ERROR(res, msg);
  }

  auto& span = span_it->second;

  if (auto type =
          utils::get_if_exists<std::string_view>(request_json, "type")) {
    if (!type->empty()) span.set_error_type(*type);
  }

  if (auto message =
          utils::get_if_exists<std::string_view>(request_json, "message")) {
    if (!message->empty()) span.set_error_message(*message);
  }

  if (auto stack =
          utils::get_if_exists<std::string_view>(request_json, "stack")) {
    if (!stack->empty()) span.set_error_stack(*stack);
  }

  res.status = 200;
}

#undef VALIDATION_ERROR
