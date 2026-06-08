#include "telemetry_impl.h"

#include <datadog/clock.h>
#include <datadog/datadog_agent_config.h>
#include <datadog/dict_writer.h>
#include <datadog/runtime_id.h>
#include <datadog/string_view.h>
#include <datadog/telemetry/telemetry.h>
#include <datadog/tracer_signature.h>

#include <chrono>

#include "datadog_agent.h"
#include "platform_util.h"

using namespace datadog::tracing;
using namespace std::chrono_literals;

namespace datadog::telemetry {
namespace internal_metrics {

/// The number of logs created with a given log level. Useful for calculating
/// impact for other features (automatic sending of logs). Levels should be one
/// of `debug`, `info`, `warn`, `error`, `critical`.
const telemetry::Counter logs_created{"logs_created", "general", true};

/// The number of requests sent to the api endpoint in the agent that errored,
/// tagged by the error type (e.g. `type:timeout`, `type:network`,
/// `type:status_code`) and Endpoint (`endpoint:agent`, `endpoint:agentless`).
const telemetry::Counter errors{"telemetry_api.errors", "telemetry", true};

/// The number of requests sent to a telemetry endpoint, regardless of success,
/// tagged by the endpoint (`endpoint:agent`, `endpoint:agentless`).
const telemetry::Counter requests{"telemetry_api.requests", "telemetry", true};

/// The number of responses received from the endpoint, tagged with status code
/// (`status_code:200`, `status_code:404`) and endpoint (`endpoint:agent`,
/// `endpoint:agentless`).
const telemetry::Counter responses{"telemetry_api.responses", "telemetry",
                                   true};

/// The size of the payload sent to the stats endpoint in bytes, tagged by the
/// endpoint (`endpoint:agent`, `endpoint:agentless`).
const telemetry::Distribution bytes_sent{"telemetry_api.bytes", "telemetry",
                                         true};

/// The time it takes to send the payload sent to the endpoint in ms, tagged by
/// the endpoint (`endpoint:agent`, `endpoint:agentless`).
const telemetry::Distribution request_duration{"telemetry_api.ms", "telemetry",
                                               true};

}  // namespace internal_metrics

namespace {

constexpr std::chrono::steady_clock::duration request_timeout = 2s;

HTTPClient::URL make_telemetry_endpoint(HTTPClient::URL url) {
  append(url.path, "/telemetry/proxy/api/v2/apmtelemetry");
  return url;
}

void cancel_tasks(std::vector<tracing::EventScheduler::Cancel>& tasks) {
  for (auto& cancel_task : tasks) {
    cancel_task();
  }
  tasks.clear();
}

std::string to_string(datadog::tracing::ConfigName name) {
  switch (name) {
    case ConfigName::SERVICE_NAME:
      return "service";
    case ConfigName::SERVICE_ENV:
      return "env";
    case ConfigName::SERVICE_VERSION:
      return "application_version";
    case ConfigName::REPORT_TRACES:
      return "trace_enabled";
    case ConfigName::TAGS:
      return "trace_tags";
    case ConfigName::EXTRACTION_STYLES:
      return "trace_propagation_style_extract";
    case ConfigName::INJECTION_STYLES:
      return "trace_propagation_style_inject";
    case ConfigName::STARTUP_LOGS:
      return "trace_startup_logs_enabled";
    case ConfigName::REPORT_TELEMETRY:
      return "instrumentation_telemetry_enabled";
    case ConfigName::DELEGATE_SAMPLING:
      return "DD_TRACE_DELEGATE_SAMPLING";
    case ConfigName::GENEREATE_128BIT_TRACE_IDS:
      return "trace_128_bits_id_enabled";
    case ConfigName::AGENT_URL:
      return "trace_agent_url";
    case ConfigName::RC_POLL_INTERVAL:
      return "remote_config_poll_interval";
    case ConfigName::TRACE_SAMPLING_RATE:
      return "trace_sample_rate";
    case ConfigName::TRACE_SAMPLING_LIMIT:
      return "trace_rate_limit";
    case ConfigName::SPAN_SAMPLING_RULES:
      return "span_sample_rules";
    case ConfigName::TRACE_SAMPLING_RULES:
      return "trace_sample_rules";
    case ConfigName::TRACE_BAGGAGE_MAX_BYTES:
      return "trace_baggage_max_bytes";
    case ConfigName::TRACE_BAGGAGE_MAX_ITEMS:
      return "trace_baggage_max_items";
    case ConfigName::APM_TRACING_ENABLED:
      return "apm_tracing_enabled";
  }

  std::abort();
}

nlohmann::json encode_logs(const std::vector<telemetry::LogMessage>& logs) {
  auto encoded_logs = nlohmann::json::array();
  for (auto& log : logs) {
    auto encoded = nlohmann::json{
        {"message", log.message},
        {"level", to_string(log.level)},
        {"tracer_time", log.timestamp},
    };
    if (log.stacktrace) {
      encoded.emplace("stack_trace", *log.stacktrace);
    }

    encoded_logs.emplace_back(std::move(encoded));
  }
  return encoded_logs;
}

std::string_view to_string(details::MetricType type) {
  using namespace datadog::telemetry::details;
  switch (type) {
    case MetricType::counter:
      return "count";
    case MetricType::rate:
      return "rate";
    case MetricType::distribution:
      return "distribution";
  }

  return "";
}

// TODO: do `enable_if`
template <typename T>
void encode_metrics(
    nlohmann::json::array_t& metrics,
    const std::unordered_map<MetricContext<T>, telemetry::MetricSnapshot>&
        counters_snapshots) {
  for (const auto& [metric_ctx, snapshots] : counters_snapshots) {
    auto encoded = nlohmann::json{
        {"metric", metric_ctx.id.name},
        {"type", to_string(metric_ctx.id.type)},
        {"common", metric_ctx.id.common},
        {"namespace", metric_ctx.id.scope},
    };

    if (!metric_ctx.tags.empty()) {
      encoded.emplace("tags", metric_ctx.tags);
    }

    auto points = nlohmann::json::array();
    for (const auto& [timestamp, value] : snapshots) {
      points.emplace_back(nlohmann::json::array({timestamp, value}));
    }

    encoded.emplace("points", points);
    metrics.emplace_back(std::move(encoded));
  }
}

nlohmann::json encode_distributions(
    const std::unordered_map<MetricContext<Distribution>,
                             std::vector<uint64_t>>& distributions) {
  auto j = nlohmann::json::array();

  for (const auto& [metric_ctx, values] : distributions) {
    auto series = nlohmann::json{
        {"metric", metric_ctx.id.name},
        {"common", metric_ctx.id.common},
        {"namespace", metric_ctx.id.scope},
        {"points", values},
    };
    if (!metric_ctx.tags.empty()) {
      series.emplace("tags", metric_ctx.tags);
    }
    j.emplace_back(series);
  }

  return j;
}

}  // namespace

Telemetry::Telemetry(FinalizedConfiguration config,
                     TracerSignature tracer_signature,
                     std::shared_ptr<tracing::Logger> logger,
                     std::shared_ptr<tracing::HTTPClient> client,
                     std::shared_ptr<tracing::EventScheduler> event_scheduler,
                     HTTPClient::URL agent_url, Clock clock)
    : config_(std::move(config)),
      logger_(std::move(logger)),
      telemetry_endpoint_(make_telemetry_endpoint(agent_url)),
      tracer_signature_(tracer_signature),
      http_client_(client),
      clock_(std::move(clock)),
      scheduler_(event_scheduler),
      host_info_(get_host_info()) {
  app_started();
  schedule_tasks();
}

void Telemetry::schedule_tasks() {
  tasks_.emplace_back(scheduler_->schedule_recurring_event(
      config_.heartbeat_interval,
      [this]() { send_payload("app-heartbeat", heartbeat_and_telemetry()); }));

  if (config_.report_metrics) {
    tasks_.emplace_back(scheduler_->schedule_recurring_event(
        config_.metrics_interval, [this]() mutable { capture_metrics(); }));
  }
}

Telemetry::~Telemetry() {
  if (!tasks_.empty()) {
    cancel_tasks(tasks_);
    app_closing();
  }
}

Telemetry::Telemetry(Telemetry&& rhs)
    : config_(std::move(rhs.config_)),
      logger_(std::move(rhs.logger_)),
      telemetry_endpoint_(std::move(rhs.telemetry_endpoint_)),
      tracer_signature_(std::move(rhs.tracer_signature_)),
      http_client_(rhs.http_client_),
      clock_(std::move(rhs.clock_)),
      scheduler_(std::move(rhs.scheduler_)),
      counters_(std::move(rhs.counters_)),
      counters_snapshot_(std::move(rhs.counters_snapshot_)),
      rates_(std::move(rhs.rates_)),
      rates_snapshot_(std::move(rhs.rates_snapshot_)),
      distributions_(std::move(rhs.distributions_)),
      seq_id_(rhs.seq_id_),
      config_seq_ids_(rhs.config_seq_ids_),
      host_info_(rhs.host_info_) {
  cancel_tasks(rhs.tasks_);
  schedule_tasks();
}

Telemetry& Telemetry::operator=(Telemetry&& rhs) {
  if (&rhs != this) {
    cancel_tasks(rhs.tasks_);
    std::swap(config_, rhs.config_);
    std::swap(logger_, rhs.logger_);
    std::swap(telemetry_endpoint_, rhs.telemetry_endpoint_);
    std::swap(http_client_, rhs.http_client_);
    std::swap(tracer_signature_, rhs.tracer_signature_);
    std::swap(http_client_, rhs.http_client_);
    std::swap(clock_, rhs.clock_);
    std::swap(scheduler_, rhs.scheduler_);
    std::swap(counters_, rhs.counters_);
    std::swap(counters_snapshot_, rhs.counters_snapshot_);
    std::swap(rates_, rhs.rates_);
    std::swap(rates_snapshot_, rhs.rates_snapshot_);
    std::swap(distributions_, rhs.distributions_);
    std::swap(seq_id_, rhs.seq_id_);
    std::swap(config_seq_ids_, rhs.config_seq_ids_);
    std::swap(host_info_, rhs.host_info_);
    schedule_tasks();
  }
  return *this;
}

void Telemetry::log_error(std::string message) {
  if (!config_.report_logs) return;
  increment_counter(internal_metrics::logs_created, {"level:error"});
  log(std::move(message), LogLevel::ERROR);
}

void Telemetry::log_error(std::string message, std::string stacktrace) {
  if (!config_.report_logs) return;
  increment_counter(internal_metrics::logs_created, {"level:error"});
  log(std::move(message), LogLevel::ERROR, stacktrace);
}

void Telemetry::log_warning(std::string message) {
  if (!config_.report_logs) return;
  increment_counter(internal_metrics::logs_created, {"level:warning"});
  log(std::move(message), LogLevel::WARNING);
}

void Telemetry::app_started() {
  auto payload = app_started_payload();

  auto on_headers = [payload_size = payload.size(),
                     debug_enabled = config_.debug](DictWriter& headers) {
    headers.set("Content-Type", "application/json");
    headers.set("Content-Length", std::to_string(payload_size));
    headers.set("DD-Telemetry-API-Version", "v2");
    headers.set("DD-Client-Library-Language", "cpp");
    headers.set("DD-Client-Library-Version", tracer_version);
    headers.set("DD-Telemetry-Request-Type", "app-started");
    if (debug_enabled) {
      headers.set("DD-Telemetry-Debug-Enabled", "true");
    }
  };

  auto on_response = [logger = logger_](int response_status, const DictReader&,
                                        std::string response_body) {
    if (response_status < 200 || response_status >= 300) {
      logger->log_error([&](auto& stream) {
        stream << "Unexpected telemetry response status " << response_status
               << " with body (if any, starts on next line):\n"
               << response_body;
      });
    }
  };

  auto on_error = [logger = logger_](Error error) {
    logger->log_error(error.with_prefix(
        "Error occurred during HTTP request for telemetry: "));
  };

  increment_counter(internal_metrics::requests, {"endpoint:agent"});
  add_datapoint(internal_metrics::bytes_sent, {"endpoint:agent"},
                payload.size());

  auto post_result =
      http_client_->post(telemetry_endpoint_, on_headers, std::move(payload),
                         std::move(on_response), std::move(on_error),
                         clock_().tick + request_timeout);
  if (auto* error = post_result.if_error()) {
    increment_counter(internal_metrics::errors,
                      {"type:network", "endpoint:agent"});
    logger_->log_error(
        error->with_prefix("Unexpected error submitting telemetry event: "));
  }
}

void Telemetry::app_closing() {
  // Capture metrics in-between two ticks to be sent with the last payload.
  capture_metrics();

  send_payload("app-closing", app_closing_payload());
  http_client_->drain(clock_().tick + request_timeout);
}

void Telemetry::send_payload(StringView request_type, std::string payload) {
  auto set_telemetry_headers = [request_type, payload_size = payload.size(),
                                debug_enabled =
                                    config_.debug](DictWriter& headers) {
    headers.set("Content-Type", "application/json");
    headers.set("Content-Length", std::to_string(payload_size));
    headers.set("DD-Telemetry-API-Version", "v2");
    headers.set("DD-Client-Library-Language", "cpp");
    headers.set("DD-Client-Library-Version", tracer_version);
    headers.set("DD-Telemetry-Request-Type", request_type);
    if (debug_enabled) {
      headers.set("DD-Telemetry-Debug-Enabled", "true");
    }
  };

  auto on_response = [this, logger = logger_](int response_status,
                                              const DictReader&,
                                              std::string response_body) {
    if (response_status >= 500) {
      increment_counter(internal_metrics::responses,
                        {"status_code:5xx", "endpoint:agent"});
    } else if (response_status >= 400) {
      increment_counter(internal_metrics::responses,
                        {"status_code:4xx", "endpoint:agent"});
    } else if (response_status >= 300) {
      increment_counter(internal_metrics::responses,
                        {"status_code:3xx", "endpoint:agent"});
    } else if (response_status >= 200) {
      increment_counter(internal_metrics::responses,
                        {"status_code:2xx", "endpoint:agent"});
    } else if (response_status >= 100) {
      increment_counter(internal_metrics::responses,
                        {"status_code:1xx", "endpoint:agent"});
    }

    if (response_status < 200 || response_status >= 300) {
      logger->log_error([&](auto& stream) {
        stream << "Unexpected telemetry response status " << response_status
               << " with body (if any, starts on next line):\n"
               << response_body;
      });
    }
  };

  // Callback for unsuccessful telemetry HTTP requests.
  auto on_error = [this, logger = logger_](Error error) {
    increment_counter(internal_metrics::errors,
                      {"type:network", "endpoint:agent"});
    logger->log_error(error.with_prefix(
        "Error occurred during HTTP request for telemetry: "));
  };

  increment_counter(internal_metrics::requests, {"endpoint:agent"});
  add_datapoint(internal_metrics::bytes_sent, {"endpoint:agent"},
                payload.size());

  auto post_result =
      http_client_->post(telemetry_endpoint_, set_telemetry_headers,
                         std::move(payload), std::move(on_response),
                         std::move(on_error), clock_().tick + request_timeout);
  if (auto* error = post_result.if_error()) {
    increment_counter(internal_metrics::errors,
                      {"type:network", "endpoint:agent"});
    logger_->log_error(
        error->with_prefix("Unexpected error submitting telemetry event: "));
  }
}

void Telemetry::send_configuration_change() {
  if (configuration_snapshot_.empty()) return;

  std::vector<ConfigMetadata> current_configuration;
  std::swap(current_configuration, configuration_snapshot_);

  auto configuration_json = nlohmann::json::array();
  for (const auto& config_metadata : current_configuration) {
    configuration_json.emplace_back(
        generate_configuration_field(config_metadata));
  }

  auto telemetry_body =
      generate_telemetry_body("app-client-configuration-change");
  telemetry_body["payload"] =
      nlohmann::json{{"configuration", configuration_json}};

  send_payload("app-client-configuration-change", telemetry_body.dump());
}

std::string Telemetry::heartbeat_and_telemetry() {
  auto batch_payloads = nlohmann::json::array();

  auto heartbeat = nlohmann::json::object({
      {"request_type", "app-heartbeat"},
  });
  batch_payloads.emplace_back(std::move(heartbeat));

  std::unordered_map<MetricContext<Distribution>, std::vector<uint64_t>>
      distributions;
  {
    std::lock_guard l{distributions_mutex_};
    std::swap(distributions_, distributions);
  }

  std::unordered_map<MetricContext<Counter>, MetricSnapshot> counters_snapshot;
  {
    std::lock_guard l{counter_mutex_};
    std::swap(counters_snapshot_, counters_snapshot);
  }

  std::unordered_map<MetricContext<Rate>, MetricSnapshot> rates_snapshot;
  {
    std::lock_guard l{rate_mutex_};
    std::swap(rates_snapshot_, rates_snapshot);
  }

  nlohmann::json::array_t metrics = nlohmann::json::array();
  encode_metrics(metrics, counters_snapshot);
  encode_metrics(metrics, rates_snapshot);
  if (!metrics.empty()) {
    auto generate_metrics = nlohmann::json::object({
        {"request_type", "generate-metrics"},
        {"payload", nlohmann::json::object({
                        {"series", metrics},
                    })},
    });
    batch_payloads.emplace_back(std::move(generate_metrics));
  }

  if (auto distributions_series = encode_distributions(distributions);
      !distributions.empty()) {
    auto distributions_json = nlohmann::json{
        {"request_type", "distributions"},
        {
            "payload",
            nlohmann::json{
                {"series", distributions_series},
            },
        },
    };
    batch_payloads.emplace_back(std::move(distributions_json));
  }

  std::vector<telemetry::LogMessage> old_logs;
  {
    std::lock_guard l{log_mutex_};
    std::swap(old_logs, logs_);
  }

  if (!old_logs.empty()) {
    auto encoded_logs = encode_logs(old_logs);
    assert(!encoded_logs.empty());

    auto logs_payload = nlohmann::json::object({
        {"request_type", "logs"},
        {"payload",
         nlohmann::json{
             {"logs", encoded_logs},
         }},
    });

    batch_payloads.emplace_back(std::move(logs_payload));
  }

  auto telemetry_body = generate_telemetry_body("message-batch");
  telemetry_body["payload"] = batch_payloads;
  auto message_batch_payload = telemetry_body.dump();

  return message_batch_payload;
}

std::string Telemetry::app_closing_payload() {
  auto batch_payloads = nlohmann::json::array();

  auto app_closing = nlohmann::json::object({
      {"request_type", "app-closing"},
  });
  batch_payloads.emplace_back(std::move(app_closing));

  nlohmann::json::array_t metrics = nlohmann::json::array();
  encode_metrics(metrics, counters_snapshot_);
  encode_metrics(metrics, rates_snapshot_);
  if (!metrics.empty()) {
    auto generate_metrics = nlohmann::json::object({
        {"request_type", "generate-metrics"},
        {"payload", nlohmann::json::object({
                        {"series", metrics},
                    })},
    });
    batch_payloads.emplace_back(std::move(generate_metrics));
  }

  if (auto distributions_series = encode_distributions(distributions_);
      !distributions_.empty()) {
    auto distributions_json = nlohmann::json{
        {"request_type", "distributions"},
        {
            "payload",
            nlohmann::json{
                {"series", distributions_series},
            },
        },
    };
    batch_payloads.emplace_back(std::move(distributions_json));
  }

  if (!logs_.empty()) {
    auto encoded_logs = encode_logs(logs_);
    assert(!encoded_logs.empty());

    auto logs_payload = nlohmann::json::object({
        {"request_type", "logs"},
        {"payload",
         nlohmann::json{
             {"logs", encoded_logs},
         }},
    });

    batch_payloads.emplace_back(std::move(logs_payload));
  }

  auto telemetry_body = generate_telemetry_body("message-batch");
  telemetry_body["payload"] = batch_payloads;

  auto message_batch_payload = telemetry_body.dump();
  return message_batch_payload;
}

std::string Telemetry::app_started_payload() {
  auto configuration_json = nlohmann::json::array();
  auto product_json = nlohmann::json::object();

  for (const auto& product : config_.products) {
    auto& configurations = product.configurations;
    for (const auto& [_, config_metadata] : configurations) {
      // if (config_metadata.value.empty()) continue;

      configuration_json.emplace_back(
          generate_configuration_field(config_metadata));
    }

    /// NOTE(@dmehala): Telemetry API is tightly related to APM tracing and
    /// assumes telemetry event can only be generated from a tracer. The
    /// assumption is that the tracing product is always enabled and there
    /// is no need to declare it.
    if (product.name == Product::Name::tracing) continue;

    auto p = nlohmann::json{
        {to_string(product.name),
         nlohmann::json{
             {"version", product.version},
             {"enabled", product.enabled},
         }},
    };

    if (product.error_code || product.error_message) {
      auto p_error = nlohmann::json{};
      if (product.error_code) {
        p_error.emplace("code", *product.error_code);
      }
      if (product.error_message) {
        p_error.emplace("message", *product.error_message);
      }

      p.emplace("error", std::move(p_error));
    }

    product_json.emplace(std::move(p));
  }

  auto app_started_msg = nlohmann::json{
      {"request_type", "app-started"},
      {
          "payload",
          nlohmann::json{
              {"configuration", configuration_json},
              {"products", product_json},
          },
      },
  };

  if (config_.install_id || config_.install_time || config_.install_type) {
    auto install_signature = nlohmann::json{};
    if (config_.install_id) {
      install_signature.emplace("install_id", *config_.install_id);
    }
    if (config_.install_type) {
      install_signature.emplace("install_type", *config_.install_type);
    }
    if (config_.install_time) {
      install_signature.emplace("install_time", *config_.install_time);
    }

    app_started_msg["payload"].emplace("install_signature",
                                       std::move(install_signature));
  }

  auto batch = generate_telemetry_body("message-batch");
  batch["payload"] = nlohmann::json::array({std::move(app_started_msg)});

  if (!config_.integration_name.empty()) {
    auto integration_msg = nlohmann::json{
        {"request_type", "app-integrations-change"},
        {
            "payload",
            nlohmann::json{
                {
                    "integrations",
                    nlohmann::json::array({
                        nlohmann::json{{"name", config_.integration_name},
                                       {"version", config_.integration_version},
                                       {"enabled", true}},
                    }),
                },
            },
        },
    };

    batch["payload"].emplace_back(std::move(integration_msg));
  }

  return batch.dump();
}

nlohmann::json Telemetry::generate_telemetry_body(std::string request_type) {
  std::time_t tracer_time = std::chrono::duration_cast<std::chrono::seconds>(
                                clock_().wall.time_since_epoch())
                                .count();
  seq_id_++;
  return nlohmann::json::object({
      {"api_version", "v2"},
      {"seq_id", seq_id_},
      {"request_type", request_type},
      {"tracer_time", tracer_time},
      {"runtime_id", tracer_signature_.runtime_id.string()},
      {"debug", config_.debug},
      {"application",
       nlohmann::json::object({
           {"service_name", tracer_signature_.default_service},
           {"env", tracer_signature_.default_environment},
           {"tracer_version", tracer_signature_.library_version},
           {"language_name", tracer_signature_.library_language},
           {"language_version", tracer_signature_.library_language_version},
       })},
      {"host",
       {
           {"hostname", host_info_.hostname},
           {"os", host_info_.os},
           {"os_version", host_info_.os_version},
           {"architecture", host_info_.cpu_architecture},
           {"kernel_name", host_info_.kernel_name},
           {"kernel_version", host_info_.kernel_version},
           {"kernel_release", host_info_.kernel_release},
       }},
  });
}

nlohmann::json Telemetry::generate_configuration_field(
    const ConfigMetadata& config_metadata) {
  // NOTE(@dmehala): `seq_id` should start at 1 so that the go backend can
  // detect between non set fields.
  config_seq_ids_[config_metadata.name] += 1;
  auto seq_id = config_seq_ids_[config_metadata.name];

  auto j = nlohmann::json{{"name", to_string(config_metadata.name)},
                          {"value", config_metadata.value},
                          {"seq_id", seq_id}};

  switch (config_metadata.origin) {
    case ConfigMetadata::Origin::ENVIRONMENT_VARIABLE:
      j["origin"] = "env_var";
      break;
    case ConfigMetadata::Origin::CODE:
      j["origin"] = "code";
      break;
    case ConfigMetadata::Origin::REMOTE_CONFIG:
      j["origin"] = "remote_config";
      break;
    case ConfigMetadata::Origin::DEFAULT:
      j["origin"] = "default";
      break;
  }

  if (config_metadata.error) {
    // clang-format off
      j["error"] = {
        {"code", config_metadata.error->code},
        {"message", config_metadata.error->message}
      };
    // clang-format on
  }

  return j;
}

void Telemetry::capture_configuration_change(
    const std::vector<tracing::ConfigMetadata>& new_configuration) {
  configuration_snapshot_.insert(configuration_snapshot_.begin(),
                                 new_configuration.begin(),
                                 new_configuration.end());
}

void Telemetry::capture_metrics() {
  std::time_t timepoint = std::chrono::duration_cast<std::chrono::seconds>(
                              clock_().wall.time_since_epoch())
                              .count();

  std::unordered_map<MetricContext<Counter>, uint64_t> counter_snapshot;
  {
    std::lock_guard l{counter_mutex_};
    std::swap(counter_snapshot, counters_);
  }

  for (auto& [counter, value] : counter_snapshot) {
    auto& counter_snapshots = counters_snapshot_[counter];
    counter_snapshots.emplace_back(std::make_pair(timepoint, value));
  }

  std::unordered_map<MetricContext<Rate>, uint64_t> rate_snapshot;
  {
    std::lock_guard l{rate_mutex_};
    std::swap(rate_snapshot, rates_);
  }

  for (auto& [rate, value] : rate_snapshot) {
    auto& rates_snapshots = rates_snapshot_[rate];
    rates_snapshots.emplace_back(std::make_pair(timepoint, value));
  }
}

void Telemetry::log(std::string message, telemetry::LogLevel level,
                    Optional<std::string> stacktrace) {
  auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                       clock_().wall.time_since_epoch())
                       .count();
  std::lock_guard l{log_mutex_};
  logs_.emplace_back(
      telemetry::LogMessage{std::move(message), level, stacktrace, timestamp});
}

void Telemetry::increment_counter(const Counter& id) {
  increment_counter(id, {});
}

void Telemetry::increment_counter(const Counter& id,
                                  const std::vector<std::string>& tags) {
  std::lock_guard l{counter_mutex_};
  counters_[{id, tags}] += 1;
}

void Telemetry::decrement_counter(const Counter& id) {
  decrement_counter(id, {});
}

void Telemetry::decrement_counter(const Counter& id,
                                  const std::vector<std::string>& tags) {
  std::lock_guard l{counter_mutex_};
  auto& v = counters_[{id, tags}];
  if (v > 0) v -= 1;
}

void Telemetry::set_counter(const Counter& id, uint64_t value) {
  set_counter(id, {}, value);
}

void Telemetry::set_counter(const Counter& id,
                            const std::vector<std::string>& tags,
                            uint64_t value) {
  std::lock_guard l{counter_mutex_};
  counters_[{id, tags}] = value;
}

void Telemetry::set_rate(const Rate& id, uint64_t value) {
  set_rate(id, {}, value);
}

void Telemetry::set_rate(const Rate& id, const std::vector<std::string>& tags,
                         uint64_t value) {
  std::lock_guard l{rate_mutex_};
  rates_[{id, tags}] = value;
}

void Telemetry::add_datapoint(const Distribution& id, uint64_t value) {
  add_datapoint(id, {}, value);
}

void Telemetry::add_datapoint(const Distribution& id,
                              const std::vector<std::string>& tags,
                              uint64_t value) {
  std::lock_guard l{distributions_mutex_};
  distributions_[{id, tags}].emplace_back(value);
}

}  // namespace datadog::telemetry
