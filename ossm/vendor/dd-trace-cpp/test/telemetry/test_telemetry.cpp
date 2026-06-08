// These are tests for `TracerTelemetry`. TracerTelemetry is used to measure
// activity in other parts of the tracer implementation, and construct messages
// that are sent to the datadog agent.

#include <datadog/clock.h>
#include <datadog/span_defaults.h>

#include <datadog/json.hpp>
#include <unordered_set>

#include "../common/environment.h"
#include "datadog/runtime_id.h"
#include "datadog/telemetry/telemetry_impl.h"
#include "mocks/http_clients.h"
#include "mocks/loggers.h"
#include "test.h"

namespace ddtest = datadog::test;
using namespace datadog::tracing;
using namespace datadog::telemetry;
using namespace std::chrono_literals;

namespace {
bool is_valid_telemetry_payload(const nlohmann::json& json) {
  return json.contains("/api_version"_json_pointer) &&
         json.at("api_version") == "v2" &&
         json.contains("/seq_id"_json_pointer) &&
         json.contains("/request_type"_json_pointer) &&
         json.contains("/tracer_time"_json_pointer) &&
         json.contains("/runtime_id"_json_pointer) &&
         json.contains("/payload"_json_pointer) &&
         json.contains("/application"_json_pointer) &&
         json.contains("/host"_json_pointer);
}

std::optional<nlohmann::json> find_payload(const nlohmann::json& messages,
                                           std::string_view kind) {
  for (const auto& m : messages) {
    if (m["request_type"].get<std::string_view>() == kind) return m;
  }

  return std::nullopt;
};

struct FakeEventScheduler : public EventScheduler {
  size_t count_tasks = 0;
  std::function<void()> heartbeat_callback = nullptr;
  std::function<void()> metrics_callback = nullptr;
  Optional<std::chrono::steady_clock::duration> heartbeat_interval;
  Optional<std::chrono::steady_clock::duration> metrics_interval;
  bool cancelled = false;

  // NOTE: White box testing. This is a limitation of the event scheduler API.
  Cancel schedule_recurring_event(std::chrono::steady_clock::duration interval,
                                  std::function<void()> callback) override {
    if (count_tasks == 0) {
      heartbeat_callback = callback;
      heartbeat_interval = interval;
    } else if (count_tasks == 1) {
      metrics_callback = callback;
      metrics_interval = interval;
    }
    count_tasks++;
    return [this]() { cancelled = true; };
  }

  void trigger_heartbeat() {
    assert(heartbeat_callback != nullptr);
    heartbeat_callback();
  }

  void trigger_metrics_capture() {
    assert(metrics_callback != nullptr);
    metrics_callback();
  }

  std::string config() const override {
    return nlohmann::json::object({{"type", "FakeEventScheduler"}}).dump();
  }
};

}  // namespace

#define TELEMETRY_IMPLEMENTATION_TEST(x) \
  TEST_CASE(x, "[telemetry],[telemetry.impl]")

TELEMETRY_IMPLEMENTATION_TEST("Tracer telemetry lifecycle") {
  auto logger = std::make_shared<MockLogger>();
  auto client = std::make_shared<MockHTTPClient>();
  auto scheduler = std::make_shared<FakeEventScheduler>();

  const TracerSignature tracer_signature{
      /* runtime_id = */ RuntimeID::generate(),
      /* service = */ "testsvc",
      /* environment = */ "test"};

  auto url = HTTPClient::URL::parse("http://localhost:8000");

  SECTION("ctor send app-started message") {
    SECTION("Without a defined integration") {
      Telemetry telemetry{*finalize_config(),
                          tracer_signature,
                          logger,
                          client,
                          scheduler,
                          *url};
      /// By default the integration is `datadog` with the tracer version.
      /// TODO: remove the default because these datadog are already part of the
      /// request header.
      auto app_started = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(app_started) == true);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].size() == 2);

      auto& app_started_payload = app_started["payload"][0];
      CHECK(app_started_payload["request_type"] == "app-started");
      CHECK(app_started_payload["payload"]["configuration"].empty());
    }

    SECTION("With an integration") {
      client->clear();

      Configuration cfg;
      cfg.integration_name = "nginx";
      cfg.integration_version = "1.25.2";
      Telemetry telemetry2{*finalize_config(cfg),
                           tracer_signature,
                           logger,
                           client,
                           scheduler,
                           *url};

      auto app_started = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(app_started) == true);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].size() == 2);

      const std::unordered_set<std::string> expected{"app-started",
                                                     "app-integrations-change"};

      for (const auto& payload : app_started["payload"]) {
        CHECK(expected.find(payload["request_type"]) != expected.cend());
      }
    }

    SECTION("With installation signature") {
      client->clear();

      ddtest::EnvGuard install_id_env("DD_INSTRUMENTATION_INSTALL_ID",
                                      "68e75c48-57ca-4a12-adfc-575c4b05fcbe");
      ddtest::EnvGuard install_type_env("DD_INSTRUMENTATION_INSTALL_TYPE",
                                        "k8s_single_step");
      ddtest::EnvGuard install_time_env("DD_INSTRUMENTATION_INSTALL_TIME",
                                        "1703188212");

      Telemetry telemetry4{*finalize_config(),
                           tracer_signature,
                           logger,
                           client,
                           scheduler,
                           *url};

      auto app_started = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(app_started) == true);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].is_array());
      REQUIRE(app_started["payload"].size() == 2);

      auto& app_started_payload = app_started["payload"][0];
      CHECK(app_started_payload["request_type"] == "app-started");

      auto install_payload =
          app_started_payload["payload"]["install_signature"];
      REQUIRE(install_payload.is_object());

      REQUIRE(install_payload.contains("install_id") == true);
      CHECK(install_payload["install_id"] ==
            "68e75c48-57ca-4a12-adfc-575c4b05fcbe");

      REQUIRE(install_payload.contains("install_id") == true);
      CHECK(install_payload["install_type"] == "k8s_single_step");

      REQUIRE(install_payload.contains("install_id") == true);
      CHECK(install_payload["install_time"] == "1703188212");
    }

    SECTION("With configuration") {
      client->clear();

      Product product;
      product.name = Product::Name::tracing;
      product.enabled = true;
      product.version = tracer_version;
      product.configurations = std::unordered_map<ConfigName, ConfigMetadata>{
          {ConfigName::SERVICE_NAME,
           ConfigMetadata(ConfigName::SERVICE_NAME, "foo",
                          ConfigMetadata::Origin::CODE)},
      };

      Configuration cfg;
      cfg.products.emplace_back(std::move(product));

      Telemetry telemetry3{*finalize_config(cfg),
                           tracer_signature,
                           logger,
                           client,
                           scheduler,
                           *url};

      auto app_started = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(app_started) == true);
      REQUIRE(app_started["request_type"] == "message-batch");
      REQUIRE(app_started["payload"].is_array());
      REQUIRE(app_started["payload"].size() == 2);

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
        SECTION("empty configuration do not generate a valid payload") {
          client->clear();
          telemetry3.send_configuration_change();

          CHECK(client->request_body.empty());
        }

        SECTION("valid configurations update") {
          const std::vector<ConfigMetadata> new_config{
              {ConfigName::SERVICE_NAME, "increase seq_id",
               ConfigMetadata::Origin::ENVIRONMENT_VARIABLE},
              {ConfigName::REPORT_TRACES, "", ConfigMetadata::Origin::DEFAULT,
               Error{Error::Code::OTHER, "empty field"}}};

          client->clear();
          telemetry3.capture_configuration_change(new_config);
          telemetry3.send_configuration_change();

          auto updates = client->request_body;
          REQUIRE(!updates.empty());
          auto config_change_message =
              nlohmann::json::parse(updates, nullptr, false);
          REQUIRE(config_change_message.is_discarded() == false);
          REQUIRE(is_valid_telemetry_payload(config_change_message) == true);

          CHECK(config_change_message["request_type"] ==
                "app-client-configuration-change");
          CHECK(config_change_message["payload"]["configuration"].is_array());
          CHECK(config_change_message["payload"]["configuration"].size() == 2);

          const std::unordered_map<std::string, nlohmann::json> expected_json{
              {
                  "service",
                  nlohmann::json{
                      {"name", "service"},
                      {"value", "increase seq_id"},
                      {"seq_id", 2},
                      {"origin", "env_var"},
                  },
              },
              {
                  "trace_enabled",
                  nlohmann::json{
                      {"name", "trace_enabled"},
                      {"value", ""},
                      {"seq_id", 1},
                      {"origin", "default"},
                      {
                          "error",
                          {
                              {"code", Error::Code::OTHER},
                              {"message", "empty field"},
                          },
                      },
                  },
              },
          };

          for (const auto& conf :
               config_change_message["payload"]["configuration"]) {
            auto it = expected_json.find(conf["name"]);
            REQUIRE(it != expected_json.cend());
            CHECK(it->second == conf);
          }

          // No update -> no configuration update
          client->clear();
          telemetry3.send_configuration_change();
          CHECK(client->request_body.empty());
        }
      }
    }
  }

  SECTION("dtor send app-closing message") {
    {
      Telemetry telemetry{*finalize_config(),
                          tracer_signature,
                          logger,
                          client,
                          scheduler,
                          *url};
      client->clear();
    }

    auto message_batch = nlohmann::json::parse(client->request_body);
    REQUIRE(is_valid_telemetry_payload(message_batch) == true);
    REQUIRE(message_batch["payload"].size() >= 1);

    auto app_closing_payload =
        find_payload(message_batch["payload"], "app-closing");
    REQUIRE(app_closing_payload.has_value());
  }
}

TELEMETRY_IMPLEMENTATION_TEST("Tracer telemetry API") {
  const Clock clock = [] {
    TimePoint result;
    result.wall = std::chrono::system_clock::from_time_t(1672484400);
    return result;
  };

  auto logger = std::make_shared<MockLogger>();
  auto client = std::make_shared<MockHTTPClient>();
  auto scheduler = std::make_shared<FakeEventScheduler>();

  const TracerSignature tracer_signature{
      /* runtime_id = */ RuntimeID::generate(),
      /* service = */ "testsvc",
      /* environment = */ "test"};

  auto url = HTTPClient::URL::parse("http://localhost:8000");

  Telemetry telemetry{*finalize_config(),
                      tracer_signature,
                      logger,
                      client,
                      scheduler,
                      *url,
                      clock};

  SECTION("generates a heartbeat message") {
    client->clear();
    scheduler->trigger_heartbeat();

    auto heartbeat_message = client->request_body;
    auto message_batch = nlohmann::json::parse(heartbeat_message);
    REQUIRE(is_valid_telemetry_payload(message_batch) == true);
    REQUIRE(message_batch["payload"].size() >= 1);

    REQUIRE(find_payload(message_batch["payload"], "app-heartbeat"));
  }

  SECTION("metrics reporting") {
    SECTION("counters are correctly serialized in generate-metrics payload") {
      client->clear();
      /// test cases for counters:
      /// - can't decrement below zero. -> is that a telemetry requirements?
      /// - rates or counter reset to zero after capture.
      const Counter my_counter{"my_counter", "counter-test", true};
      telemetry.increment_counter(my_counter);  // = 1
      telemetry.increment_counter(my_counter);  // = 2
      telemetry.increment_counter(my_counter);  // = 3
      telemetry.decrement_counter(my_counter);  // = 2
      scheduler->trigger_metrics_capture();

      telemetry.increment_counter(my_counter);  // = 1
      scheduler->trigger_metrics_capture();

      telemetry.set_counter(my_counter, 42);
      telemetry.set_counter(my_counter, {"event:test"}, 100);
      telemetry.decrement_counter(my_counter, {"event:test"});
      scheduler->trigger_metrics_capture();

      // Expect 2 series:
      //   - `my_counter` without tags: 3 datapoint (2, 1, 42) with the same
      //   timestamp.
      //   - `my_counter` with `event:test` tags: 1 datapoint (99).
      scheduler->trigger_heartbeat();

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch) == true);
      REQUIRE(message_batch["payload"].size() >= 2);

      auto generate_metrics =
          find_payload(message_batch["payload"], "generate-metrics");
      REQUIRE(generate_metrics.has_value());
      auto payload = (*generate_metrics)["payload"];

      auto series = payload["series"];
      REQUIRE(series.size() >= 2);

      const auto expected_metrics = nlohmann::json::parse(R"(
        [
          {
            "common": true,
            "metric": "my_counter",
            "namespace": "counter-test",
            "points": [
              [ 1672484400, 99 ]
            ],
            "tags": [ "event:test" ],
            "type": "count"
          },
          {
            "common": true,
            "metric": "my_counter",
            "namespace": "counter-test",
            "points": [
              [ 1672484400, 2 ],
              [ 1672484400, 1 ],
              [ 1672484400, 42 ]
            ],
            "type": "count"
          }
        ]
      )");

      for (const auto& s : series) {
        if (s["metric"] == "my_counter") {
          if (s.contains("tags")) {
            CHECK(s == expected_metrics[0]);
          } else {
            CHECK(s == expected_metrics[1]);
          }
        }
      }

      // Make sure the next heartbeat doesn't contains counters if no
      // datapoint has been incremented, decremented or set.
      client->clear();
      scheduler->trigger_heartbeat();

      auto message_batch2 = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch2) == true);
      REQUIRE(message_batch2["payload"].size() >= 1);

      CHECK(find_payload(message_batch["payload"], "app-heartbeat"));
    }

    SECTION("counters can't go below zero") {
      client->clear();
      const Counter positive_counter{"positive_counter", "counter-test2", true};
      telemetry.decrement_counter(positive_counter);  // = 0
      telemetry.decrement_counter(positive_counter);  // = 0
      telemetry.decrement_counter(positive_counter);  // = 0

      scheduler->trigger_metrics_capture();
      scheduler->trigger_heartbeat();

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch) == true);
      REQUIRE(message_batch["payload"].size() >= 2);

      auto generate_metrics =
          find_payload(message_batch["payload"], "generate-metrics");
      REQUIRE(generate_metrics);
      auto payload = (*generate_metrics)["payload"];

      auto series = payload["series"];
      REQUIRE(series.size() >= 1);

      const auto expected_metric = nlohmann::json::parse(R"(
          {
            "common": true,
            "metric": "positive_counter",
            "namespace": "counter-test2",
            "points": [
              [ 1672484400, 0 ]
            ],
            "type": "count"
          }
      )");

      for (const auto& s : series) {
        if (s["metric"] == "positive_counter") {
          CHECK(s == expected_metric);
        }
      }
    }

    SECTION("rate") {
      client->clear();

      Rate rps{"request", "rate-test", true};
      telemetry.set_rate(rps, 1000);

      scheduler->trigger_metrics_capture();

      telemetry.set_rate(rps, 2000);
      telemetry.set_rate(rps, 5000);
      telemetry.set_rate(rps, {"status:2xx"}, 5000);

      scheduler->trigger_metrics_capture();

      // Expect 2 series:
      //  - `request` without tags: 2 datapoint (1000, 5000)
      //  - `request` with tags: 1 datapoint (5000)
      scheduler->trigger_heartbeat();

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch) == true);
      REQUIRE(message_batch["payload"].size() >= 2);
      auto generate_metrics =
          find_payload(message_batch["payload"], "generate-metrics");
      REQUIRE(generate_metrics);
      auto payload = (*generate_metrics)["payload"];

      auto series = payload["series"];
      REQUIRE(series.size() >= 2);

      const auto expected_metrics = nlohmann::json::parse(R"(
        [
          {
            "common": true,
            "metric": "request",
            "namespace": "rate-test",
            "points": [
              [ 1672484400, 5000 ]
            ],
            "tags": [ "status:2xx" ],
            "type": "rate"
          },
          {
            "common": true,
            "metric": "request",
            "namespace": "rate-test",
            "points": [
              [ 1672484400, 1000 ],
              [ 1672484400, 5000 ]
            ],
            "type": "rate"
          }
        ]
      )");

      for (const auto& s : series) {
        if (s["metric"] == "request") {
          if (s.contains("tags")) {
            CHECK(s == expected_metrics[0]);
          } else {
            CHECK(s == expected_metrics[1]);
          }
        }
      }

      // Make sure the next heartbeat doesn't contains distributions if no
      // datapoint has been added to a distribution.
      client->clear();
      scheduler->trigger_heartbeat();

      auto message_batch2 = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch2) == true);
      REQUIRE(message_batch2["payload"].size() >= 1);
      CHECK(find_payload(message_batch["payload"], "app-heartbeat"));
    }

    SECTION("distribution") {
      client->clear();

      Distribution response_time{"response_time", "dist-test", false};
      telemetry.add_datapoint(response_time, 128);
      telemetry.add_datapoint(response_time, 42);
      telemetry.add_datapoint(response_time, 3000);

      // Add a tag, this will add a new serie to the distribution payload.
      telemetry.add_datapoint(response_time, {"status:200", "method:GET"},
                              6530);

      Distribution request_size{"request_size", "dist-test-2", true};
      telemetry.add_datapoint(request_size, 1843);
      telemetry.add_datapoint(request_size, 4135);

      // Expect 3 series:
      //  - `response_time` without tags: 3 datapoint (128, 42, 3000).
      //  - `response_time` with 2 tags: 1 datapoint (6530).
      //  - `request_size`: 2 datapoint (1843, 4135).
      scheduler->trigger_heartbeat();

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch) == true);
      REQUIRE(message_batch["payload"].size() == 2);

      auto distribution_message = message_batch["payload"][1];
      REQUIRE(distribution_message["request_type"] == "distributions");

      auto distribution_series = distribution_message["payload"]["series"];
      REQUIRE(distribution_series.size() >= 3);

      const auto expected_series = nlohmann::json::parse(R"([
        {
           "common":false,
           "metric":"response_time",
           "namespace":"dist-test",
           "points": [6530],
           "tags":["status:200","method:GET"]
        },
        {
           "common":true,
           "metric": "request_size",
           "namespace":"dist-test-2",
           "points":[1843,4135]
        },
        {
           "common": false,
           "metric":"response_time",
           "namespace":"dist-test",
           "points":[128,42,3000]
        }
      ])");

      for (const auto& s : distribution_series) {
        if (s["metric"] == "response_time") {
          if (s.contains("tags")) {
            CHECK(s == expected_series[0]);
          } else {
            CHECK(s == expected_series[2]);
          }
        } else if (s["metric"] == "request_size") {
          CHECK(s == expected_series[1]);
        }
      }

      // Make sure the next heartbeat doesn't contains distributions if no
      // datapoint has been added to a distribution.
      client->clear();
      scheduler->trigger_heartbeat();

      auto message_batch2 = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch2) == true);
      REQUIRE(message_batch2["payload"].size() >= 1);
      CHECK(find_payload(message_batch["payload"], "app-heartbeat"));
    }

    SECTION("dtor sends metrics and distributions") {
      // metrics captured before the aggregation task
      const Distribution response_time{"response_time", "dist-test", false};
      const Rate rps{"request", "rate-test", true};
      const Counter my_counter{"my_counter", "counter-test", true};
      {
        Telemetry tmp_telemetry{*finalize_config(),
                                tracer_signature,
                                logger,
                                client,
                                scheduler,
                                *url,
                                clock};
        tmp_telemetry.increment_counter(my_counter);  // = 1
        tmp_telemetry.add_datapoint(response_time, 128);
        tmp_telemetry.set_rate(rps, 1000);
        client->clear();
      }

      // Expect 2 metrics with 1 datapoint each and 1 ditribution
      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch) == true);
      REQUIRE(message_batch["payload"].size() == 3);

      for (const auto& payload : message_batch["payload"]) {
        const auto& req_type = payload["request_type"];
        if (req_type == "generate-metrics") {
          const auto& metrics_series = payload["payload"]["series"];
          REQUIRE(metrics_series.size() >= 2);

          for (const auto& s : metrics_series) {
            if (s["metric"] == "my_counter") {
              const auto expected_counter = nlohmann::json::parse(R"(
                {
                  "common":true,
                  "metric":"my_counter",
                  "namespace":"counter-test",
                  "type": "count",
                  "points": [[1672484400, 1]]
                }
              )");
              CHECK(s == expected_counter);
            } else if (s["metric"] == "request") {
              const auto expected_rate = nlohmann::json::parse(R"(
                {
                  "common":true,
                  "metric":"request",
                  "namespace":"rate-test",
                  "type": "rate",
                  "points": [[1672484400, 1000]]
                }
              )");
              CHECK(s == expected_rate);
            }
          }
        } else if (req_type == "distributions") {
          const auto& distribution_series = payload["payload"]["series"];
          REQUIRE(distribution_series.size() >= 1);

          const auto expected_d0 = nlohmann::json::parse(R"(
            {
              "common":false,
              "metric":"response_time",
              "namespace":"dist-test",
              "points": [128]
            }
          )");

          for (const auto& d : distribution_series) {
            if (d["metric"] == "response_time") {
              CHECK(d == expected_d0);
            };
          }
        }
      }
    }
  }

  SECTION("logs reporting") {
    SECTION("log level is correct") {
      struct TestCase {
        std::string_view name;
        std::string input;
        Optional<std::string> stacktrace;
        std::function<void(Telemetry&, const std::string&,
                           const Optional<std::string>& stacktrace)>
            apply;
        std::string expected_log_level;
      };

      auto test_case = GENERATE(values<TestCase>({
          {
              "warning log",
              "This is a warning log!",
              nullopt,
              [](Telemetry& telemetry, const std::string& input,
                 const Optional<std::string>&) {
                telemetry.log_warning(input);
              },
              "WARNING",
          },
          {
              "error log",
              "This is an error log!",
              nullopt,
              [](Telemetry& telemetry, const std::string& input,
                 const Optional<std::string>&) { telemetry.log_error(input); },
              "ERROR",
          },
          {
              "error log with stacktrace",
              "This is an error log with a fake stacktrace!",
              "error here\nthen here\nfinally here\n",
              [](Telemetry& telemetry, const std::string& input,
                 Optional<std::string> stacktrace) {
                telemetry.log_error(input, *stacktrace);
              },
              "ERROR",
          },
      }));

      CAPTURE(test_case.name);

      client->clear();
      test_case.apply(telemetry, test_case.input, test_case.stacktrace);
      scheduler->trigger_heartbeat();

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch));
      REQUIRE(message_batch["payload"].size() >= 2);

      auto logs_message = find_payload(message_batch["payload"], "logs");
      REQUIRE(logs_message);

      auto logs_payload = (*logs_message)["payload"]["logs"];
      REQUIRE(logs_payload.size() == 1);
      CHECK(logs_payload[0]["level"] == test_case.expected_log_level);
      CHECK(logs_payload[0]["message"] == test_case.input);
      CHECK(logs_payload[0]["tracer_time"] == 1672484400);

      if (test_case.stacktrace) {
        CHECK(logs_payload[0]["stack_trace"] == test_case.stacktrace);
      } else {
        CHECK(logs_payload[0].contains("stack_trace") == false);
      }

      // Make sure the next heartbeat doesn't contains counters if no
      // datapoint has been incremented, decremented or set.
      client->clear();
      scheduler->trigger_heartbeat();

      auto message_batch2 = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch2) == true);
      REQUIRE(message_batch2["payload"].size() >= 1);
      CHECK(find_payload(message_batch["payload"], "app-heartbeat"));
    }

    SECTION("dtor sends logs in `app-closing` message") {
      {
        Telemetry tmp_telemetry{*finalize_config(),
                                tracer_signature,
                                logger,
                                client,
                                scheduler,
                                *url,
                                clock};
        tmp_telemetry.log_warning("Be careful!");
        client->clear();
      }

      auto message_batch = nlohmann::json::parse(client->request_body);
      REQUIRE(is_valid_telemetry_payload(message_batch));
      REQUIRE(message_batch["payload"].size() >= 2);

      auto logs_message = find_payload(message_batch["payload"], "logs");
      REQUIRE(logs_message);

      auto logs_payload = (*logs_message)["payload"]["logs"];
      REQUIRE(logs_payload.size() == 1);
      CHECK(logs_payload[0]["level"] == "WARNING");
      CHECK(logs_payload[0]["message"] == "Be careful!");
      CHECK(logs_payload[0]["tracer_time"] == 1672484400);
    }
  }
}

TELEMETRY_IMPLEMENTATION_TEST("Tracer telemetry configuration") {
  // Cases:
  //  - when `report_metrics` is set to false. No metrics are reported.
  //  - when `report_logs` is set to false. No logs are reported.
  //  - respects interval defined.
  //  - telemetry disabled doesn't send anything.

  auto logger = std::make_shared<MockLogger>();
  auto client = std::make_shared<MockHTTPClient>();
  auto scheduler = std::make_shared<FakeEventScheduler>();

  const TracerSignature tracer_signature{
      /* runtime_id = */ RuntimeID::generate(),
      /* service = */ "testsvc",
      /* environment = */ "test"};

  auto url = HTTPClient::URL::parse("http://localhost:8000");

  SECTION("disabling metrics reporting do not collect metrics") {
    Configuration cfg;
    cfg.report_metrics = false;

    auto final_cfg = finalize_config(cfg);
    REQUIRE(final_cfg);

    Telemetry telemetry(*final_cfg, tracer_signature, logger, client, scheduler,
                        *url);
    CHECK(scheduler->metrics_callback == nullptr);
    CHECK(scheduler->metrics_interval == nullopt);
  }

  SECTION("intervals are respected") {
    Configuration cfg;
    cfg.metrics_interval_seconds = .5;
    cfg.heartbeat_interval_seconds = 30;

    auto final_cfg = finalize_config(cfg);
    REQUIRE(final_cfg);

    Telemetry telemetry(*final_cfg, tracer_signature, logger, client, scheduler,
                        *url);
    CHECK(scheduler->metrics_callback != nullptr);
    CHECK(scheduler->metrics_interval == 500ms);

    CHECK(scheduler->heartbeat_callback != nullptr);
    CHECK(scheduler->metrics_interval != 30s);
  }

  SECTION("disabling logs reporting do not collect logs") {
    client->clear();

    Configuration cfg;
    cfg.report_logs = false;

    auto final_cfg = finalize_config(cfg);
    REQUIRE(final_cfg);

    Telemetry telemetry(*final_cfg, tracer_signature, logger, client, scheduler,
                        *url);
    telemetry.log_error("error");

    // NOTE(@dmehala): logs are sent with an heartbeat.
    scheduler->trigger_heartbeat();

    auto message_batch = nlohmann::json::parse(client->request_body);
    REQUIRE(is_valid_telemetry_payload(message_batch));
    REQUIRE(message_batch["payload"].size() >= 1);
    CHECK(find_payload(message_batch["payload"], "app-heartbeat"));
  }
}
