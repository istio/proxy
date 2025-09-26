// These are tests for `TracerTelemetry`. TracerTelemetry is used to measure
// activity in other parts of the tracer implementation, and construct messages
// that are sent to the datadog agent.

#include <datadog/span_defaults.h>
#include <datadog/tracer_telemetry.h>

#include <datadog/json.hpp>
#include <unordered_set>

#include "datadog/runtime_id.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog::tracing;

TEST_CASE("Tracer telemetry", "[telemetry]") {
  const std::time_t mock_time = 1672484400;
  const Clock clock = [mock_time]() {
    TimePoint result;
    result.wall = std::chrono::system_clock::from_time_t(mock_time);
    return result;
  };
  auto logger = std::make_shared<MockLogger>();

  const TracerSignature tracer_signature{
      /* runtime_id = */ RuntimeID::generate(),
      /* service = */ "testsvc",
      /* environment = */ "test"};

  const std::string ignore{""};

  TracerTelemetry tracer_telemetry{true,   clock, logger, tracer_signature,
                                   ignore, ignore};

  SECTION("generates app-started message") {
    SECTION("Without a defined integration") {
      auto app_started_message = tracer_telemetry.app_started({});
      auto app_started = nlohmann::json::parse(app_started_message);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].size() == 1);

      auto& app_started_payload = app_started["payload"][0];
      CHECK(app_started_payload["request_type"] == "app-started");
      CHECK(app_started_payload["payload"]["configuration"].empty());
    }

    SECTION("With an integration") {
      TracerTelemetry tracer_telemetry{
          true, clock, logger, tracer_signature, "nginx", "1.25.2"};
      auto app_started_message = tracer_telemetry.app_started({});
      auto app_started = nlohmann::json::parse(app_started_message);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].size() == 2);

      const std::unordered_set<std::string> expected{"app-started",
                                                     "app-integrations-change"};

      for (const auto& payload : app_started["payload"]) {
        CHECK(expected.find(payload["request_type"]) != expected.cend());
      }
    }

    SECTION("With configuration") {
      std::unordered_map<ConfigName, ConfigMetadata> configuration{
          {ConfigName::SERVICE_NAME,
           ConfigMetadata(ConfigName::SERVICE_NAME, "foo",
                          ConfigMetadata::Origin::CODE)}};

      auto app_started_message = tracer_telemetry.app_started(configuration);

      auto app_started = nlohmann::json::parse(app_started_message);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].is_array());
      REQUIRE(app_started["payload"].size() == 1);

      auto& app_started_payload = app_started["payload"][0];
      CHECK(app_started_payload["request_type"] == "app-started");

      auto cfg_payload = app_started_payload["payload"]["configuration"];
      REQUIRE(cfg_payload.is_array());
      REQUIRE(cfg_payload.size() == 1);

      // clang-format off
      const auto expected_conf = nlohmann::json({
        {"name", "service"},
        {"value", "foo"},
        {"seq_id", 1},
        {"origin", "code"},
      });
      // clang-format on

      CHECK(cfg_payload[0] == expected_conf);

      SECTION("generates a configuration change event") {
        SECTION("empty configuration generate a valid payload") {
          auto config_change_message = nlohmann::json::parse(
              tracer_telemetry.configuration_change({}), nullptr, false);
          REQUIRE(config_change_message.is_discarded() == false);

          CHECK(config_change_message["request_type"] ==
                "app-client-configuration-change");
          CHECK(config_change_message["payload"]["configuration"].is_array());
          CHECK(config_change_message["payload"]["configuration"].empty());
        }

        SECTION("valid configurations update") {
          const std::vector<ConfigMetadata> new_config{
              {ConfigName::SERVICE_NAME, "increase seq_id",
               ConfigMetadata::Origin::ENVIRONMENT_VARIABLE},
              {ConfigName::REPORT_TRACES, "", ConfigMetadata::Origin::DEFAULT,
               Error{Error::Code::OTHER, "empty field"}}};

          auto config_change_message = nlohmann::json::parse(
              tracer_telemetry.configuration_change(new_config), nullptr,
              false);
          REQUIRE(config_change_message.is_discarded() == false);

          CHECK(config_change_message["request_type"] ==
                "app-client-configuration-change");
          CHECK(config_change_message["payload"]["configuration"].is_array());
          CHECK(config_change_message["payload"]["configuration"].size() == 2);

          const std::unordered_map<std::string, nlohmann::json> expected_json{
              {"service", nlohmann::json{{"name", "service"},
                                         {"value", "increase seq_id"},
                                         {"seq_id", 2},
                                         {"origin", "env_var"}}},
              {"trace_enabled",
               nlohmann::json{{"name", "trace_enabled"},
                              {"value", ""},
                              {"seq_id", 1},
                              {"origin", "default"},
                              {"error",
                               {{"code", Error::Code::OTHER},
                                {"message", "empty field"}}}}}};

          for (const auto& conf :
               config_change_message["payload"]["configuration"]) {
            auto expected_conf = expected_json.find(conf["name"]);
            REQUIRE(expected_conf != expected_json.cend());
            CHECK(expected_conf->second == conf);
          }
        }
      }
    }
  }

  SECTION("generates a heartbeat message") {
    auto heartbeat_message = tracer_telemetry.heartbeat_and_telemetry();
    auto message_batch = nlohmann::json::parse(heartbeat_message);
    REQUIRE(message_batch["payload"].size() == 1);
    auto heartbeat = message_batch["payload"][0];
    REQUIRE(heartbeat["request_type"] == "app-heartbeat");
  }

  SECTION("captures metrics and sends generate-metrics payload") {
    tracer_telemetry.metrics().tracer.trace_segments_created_new.inc();
    REQUIRE(
        tracer_telemetry.metrics().tracer.trace_segments_created_new.value() ==
        1);
    tracer_telemetry.capture_metrics();
    REQUIRE(
        tracer_telemetry.metrics().tracer.trace_segments_created_new.value() ==
        0);
    auto heartbeat_and_telemetry_message =
        tracer_telemetry.heartbeat_and_telemetry();
    auto message_batch = nlohmann::json::parse(heartbeat_and_telemetry_message);
    REQUIRE(message_batch["payload"].size() == 2);
    auto generate_metrics = message_batch["payload"][1];
    REQUIRE(generate_metrics["request_type"] == "generate-metrics");
    auto payload = generate_metrics["payload"];
    auto series = payload["series"];
    REQUIRE(series.size() == 1);
    auto metric = series[0];
    REQUIRE(metric["metric"] == "trace_segments_created");
    auto tags = metric["tags"];
    REQUIRE(tags.size() == 1);
    REQUIRE(tags[0] == "new_continued:new");
    auto points = metric["points"];
    REQUIRE(points.size() == 1);
    REQUIRE(points[0][0] == mock_time);
    REQUIRE(points[0][1] == 1);
  }

  SECTION("generates an app-closing event") {
    auto app_closing_message = tracer_telemetry.app_closing();
    auto message_batch = nlohmann::json::parse(app_closing_message);
    REQUIRE(message_batch["payload"].size() == 1);
    auto heartbeat = message_batch["payload"][0];
    REQUIRE(heartbeat["request_type"] == "app-closing");
  }
}
