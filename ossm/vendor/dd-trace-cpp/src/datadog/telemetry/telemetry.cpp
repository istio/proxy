#include <datadog/optional.h>
#include <datadog/telemetry/telemetry.h>

#include "telemetry_impl.h"

namespace datadog::telemetry {
namespace details {

/// NOTE(@dmehala): Generic overload function (P0051R3) like implementation.
template <typename... Ts>
struct Overload : Ts... {
  using Ts::operator()...;
};

/// NOTE(@dmehala): Guide required for C++17. Remove once we switch to C++20.
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

}  // namespace details

using NoopTelemetry = std::monostate;

/// `TelemetryProxy` holds either the real implementation or a no-op
/// implementation.
using TelemetryProxy = std::variant<NoopTelemetry, Telemetry>;

/// NOTE(@dmehala): Here to facilitate Meyer's singleton construction.
struct Ctor_param final {
  FinalizedConfiguration configuration;
  tracing::TracerSignature tracer_signature;
  std::shared_ptr<tracing::Logger> logger;
  std::shared_ptr<tracing::HTTPClient> client;
  std::shared_ptr<tracing::EventScheduler> scheduler;
  tracing::HTTPClient::URL agent_url;
  tracing::Clock clock = tracing::default_clock;
};

TelemetryProxy make_telemetry(const tracing::Optional<Ctor_param>& init) {
  if (!init || !init->configuration.enabled) return NoopTelemetry{};
  return Telemetry{init->configuration, init->tracer_signature, init->logger,
                   init->client,        init->scheduler,        init->agent_url,
                   init->clock};
}

TelemetryProxy& instance(
    const tracing::Optional<Ctor_param>& init = tracing::nullopt) {
  static TelemetryProxy telemetry = make_telemetry(init);
  return telemetry;
}

void init(FinalizedConfiguration configuration,
          std::shared_ptr<tracing::Logger> logger,
          std::shared_ptr<tracing::HTTPClient> client,
          std::shared_ptr<tracing::EventScheduler> event_scheduler,
          tracing::HTTPClient::URL agent_url, tracing::Clock clock) {
  instance(Ctor_param{configuration,
                      tracing::TracerSignature(tracing::RuntimeID::generate(),
                                               tracing::get_process_name(), ""),
                      logger, client, event_scheduler, agent_url, clock});
}

void init(FinalizedConfiguration configuration,
          tracing::TracerSignature tracer_signature,
          std::shared_ptr<tracing::Logger> logger,
          std::shared_ptr<tracing::HTTPClient> client,
          std::shared_ptr<tracing::EventScheduler> event_scheduler,
          tracing::HTTPClient::URL agent_url, tracing::Clock clock) {
  instance(Ctor_param{configuration, tracer_signature, logger, client,
                      event_scheduler, agent_url, clock});
}

void send_configuration_change() {
  std::visit(
      details::Overload{
          [&](Telemetry& telemetry) { telemetry.send_configuration_change(); },
          [](NoopTelemetry) {},
      },
      instance());
}

void capture_configuration_change(
    const std::vector<tracing::ConfigMetadata>& new_configuration) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.capture_configuration_change(new_configuration);
                 },
                 [](NoopTelemetry) {},
             },
             instance());
}

namespace log {
void warning(std::string message) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) { telemetry.log_warning(message); },
                 [](NoopTelemetry) {},
             },
             instance());
}

void error(std::string message) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) { telemetry.log_error(message); },
                 [](NoopTelemetry) {},
             },
             instance());
}

void error(std::string message, std::string stacktrace) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.log_error(message, stacktrace);
                 },
                 [](auto&&) {},
             },
             instance());
}
}  // namespace log

namespace counter {
void increment(const Counter& counter) {
  std::visit(
      details::Overload{
          [&](Telemetry& telemetry) { telemetry.increment_counter(counter); },
          [](auto&&) {},
      },
      instance());
}

void increment(const Counter& counter, const std::vector<std::string>& tags) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.increment_counter(counter, tags);
                 },
                 [](auto&&) {},
             },
             instance());
}

void decrement(const Counter& counter) {
  std::visit(
      details::Overload{
          [&](Telemetry& telemetry) { telemetry.decrement_counter(counter); },
          [](auto&&) {},
      },
      instance());
}

void decrement(const Counter& counter, const std::vector<std::string>& tags) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.decrement_counter(counter, tags);
                 },
                 [](auto&&) {},
             },
             instance());
}

void set(const Counter& counter, uint64_t value) {
  std::visit(
      details::Overload{
          [&](Telemetry& telemetry) { telemetry.set_counter(counter, value); },
          [](auto&&) {},
      },
      instance());
}

void set(const Counter& counter, const std::vector<std::string>& tags,
         uint64_t value) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.set_counter(counter, tags, value);
                 },
                 [](auto&&) {},
             },
             instance());
}

}  // namespace counter

namespace rate {
void set(const Rate& rate, uint64_t value) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) { telemetry.set_rate(rate, value); },
                 [](auto&&) {},
             },
             instance());
}

void set(const Rate& rate, const std::vector<std::string>& tags,
         uint64_t value) {
  std::visit(
      details::Overload{
          [&](Telemetry& telemetry) { telemetry.set_rate(rate, tags, value); },
          [](auto&&) {},
      },
      instance());
}
}  // namespace rate

namespace distribution {

void add(const Distribution& distribution, uint64_t value) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.add_datapoint(distribution, value);
                 },
                 [](auto&&) {},
             },
             instance());
}

void add(const Distribution& distribution, const std::vector<std::string>& tags,
         uint64_t value) {
  std::visit(details::Overload{
                 [&](Telemetry& telemetry) {
                   telemetry.add_datapoint(distribution, tags, value);
                 },
                 [](auto&&) {},
             },
             instance());
}

}  // namespace distribution

}  // namespace datadog::telemetry
