#include <datadog/collector_response.h>
#include <datadog/config_manager.h>
#include <datadog/datadog_agent.h>
#include <datadog/datadog_agent_config.h>
#include <datadog/telemetry/telemetry.h>
#include <datadog/tracer.h>
#include <datadog/tracer_config.h>

#include <chrono>
#include <iostream>

#include "mocks/event_schedulers.h"
#include "mocks/http_clients.h"
#include "mocks/loggers.h"
#include "test.h"

using namespace datadog;
using namespace datadog::tracing;
using namespace std::chrono_literals;

#define DATADOG_AGENT_TEST(x) TEST_CASE(x, "[datadog_agent]")

DATADOG_AGENT_TEST("CollectorResponse") {
  TracerConfig config;
  config.service = "testsvc";
  const auto logger =
      std::make_shared<MockLogger>(std::cerr, MockLogger::ERRORS_ONLY);
  const auto event_scheduler = std::make_shared<MockEventScheduler>();
  const auto http_client = std::make_shared<MockHTTPClient>();
  config.logger = logger;
  config.agent.event_scheduler = event_scheduler;
  config.agent.http_client = http_client;
  // Tests currently only cover sending traces to the agent.
  // Submiting telemetry performs essentially the same steps, but may be added
  // in the future.
  config.telemetry.enabled = false;
  auto finalized = finalize_config(config);
  REQUIRE(finalized);

  SECTION("empty object is valid") {
    {
      http_client->response_status = 200;
      http_client->response_body << "{}";
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 0);
  }

  SECTION("just the default key") {
    {
      http_client->response_status = 200;
      http_client->response_body << "{\"rate_by_service\": {\""
                                 << CollectorResponse::key_of_default_rate
                                 << "\": 1.0}}";
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 0);
  }

  SECTION("default key and another key") {
    {
      http_client->response_status = 200;
      http_client->response_body
          << "{\"rate_by_service\": {\""
          << CollectorResponse::key_of_default_rate
          << "\": 1.0, \"service:wiggle,env:foo\": 0.0}}";
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 0);
  }

  SECTION("HTTP success with empty body") {
    // Don't echo error messages.
    logger->echo = nullptr;

    {
      http_client->response_status = 200;
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }

    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 1);
  }

  SECTION("invalid responses") {
    // Don't echo error messages.
    logger->echo = nullptr;

    struct TestCase {
      std::string name;
      std::string response_body;
    };

    auto test_case = GENERATE(values<TestCase>({
        {"not JSON", "well that's not right at all!"},
        {"not an object", "[\"wrong\", \"type\", 123]"},
        {"rate_by_service not an object", "{\"rate_by_service\": null}"},
        {"sample rate not a number",
         "{\"rate_by_service\": {\"service:foo,env:bar\": []}}"},
        {"invalid sample rate",
         "{\"rate_by_service\": {\"service:foo,env:bar\": -1.337}}"},
    }));

    CAPTURE(test_case.name);
    {
      http_client->response_status = 200;
      http_client->response_body << test_case.response_body;
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 1);
  }

  SECTION("HTTP non-success response code") {
    // Don't echo error messages.
    logger->echo = nullptr;

    // Datadog Agent only returns 200 on success.
    auto status = GENERATE(range(201, 600));
    {
      http_client->response_status = status;
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    REQUIRE(logger->error_count() == 1);
  }

  SECTION("HTTP client failure") {
    // Don't echo error messages.
    logger->echo = nullptr;

    const Error error{Error::OTHER, "oh no!"};
    {
      http_client->response_error = error;
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    CAPTURE(logger->entries);
    REQUIRE(logger->error_count() == 1);
    REQUIRE(logger->first_error().code == error.code);
  }

  SECTION("HTTPClient post() failure") {
    // Don't echo error messages.
    logger->echo = nullptr;

    const Error error{Error::OTHER, "oh no!"};
    {
      http_client->post_error = error;
      Tracer tracer{*finalized};
      auto span = tracer.create_span();
      (void)span;
    }
    REQUIRE(event_scheduler->cancelled);
    // REVIEW: this fails since the addition of telemetry
    REQUIRE(logger->error_count() == 1);
    REQUIRE(logger->first_error().code == error.code);
  }
}

// NOTE: `report_telemetry` is too vague for now.
// Does it mean no telemetry at all or just metrics are not generated?
//
// TODO: Use cases to implement:
//   - telemetry is disabled, no event scheduled.
//   - telemetry is enabled, after x sec generate metrics is called.
//   - send_app_started?
DATADOG_AGENT_TEST("Remote Configuration") {
  const auto logger =
      std::make_shared<MockLogger>(std::cerr, MockLogger::ERRORS_ONLY);
  logger->echo = nullptr;
  const auto event_scheduler = std::make_shared<MockEventScheduler>();
  const auto http_client = std::make_shared<MockHTTPClient>();

  TracerConfig config;
  config.service = "testsvc";
  config.logger = logger;
  config.agent.event_scheduler = event_scheduler;
  config.agent.http_client = http_client;
  config.telemetry.enabled = false;

  auto finalized = finalize_config(config);
  REQUIRE(finalized);

  const TracerSignature signature(RuntimeID::generate(), "testsvc", "test");

  // TODO: set telemetry mock
  auto config_manager = std::make_shared<ConfigManager>(*finalized);

  const auto& agent_config =
      std::get<FinalizedDatadogAgentConfig>(finalized->collector);
  DatadogAgent agent(agent_config, config.logger, signature, {});

  SECTION("404 do not log an error") {
    http_client->response_status = 404;
    agent.get_and_apply_remote_configuration_updates();
    http_client->drain(std::chrono::steady_clock::now());
    CHECK(logger->error_count() == 0);
  }

  SECTION("5xx log an error") {
    http_client->response_status = 500;
    agent.get_and_apply_remote_configuration_updates();
    http_client->drain(std::chrono::steady_clock::now());
    CHECK(logger->error_count() == 1);
  }

  SECTION("non json input") {
    http_client->response_status = 200;
    http_client->response_body << "hello, mars!";

    agent.get_and_apply_remote_configuration_updates();
    http_client->drain(std::chrono::steady_clock::now());
    CHECK(logger->error_count() == 1);
  }
}

DATADOG_AGENT_TEST("Datadog-Client-Computed-Stats header") {
  const auto logger =
      std::make_shared<MockLogger>(std::cerr, MockLogger::ERRORS_ONLY);
  const auto event_scheduler = std::make_shared<MockEventScheduler>();
  const auto http_client = std::make_shared<MockHTTPClient>();

  TracerConfig config;
  config.service = "testsvc";
  config.logger = logger;
  config.agent.event_scheduler = event_scheduler;
  config.agent.http_client = http_client;
  config.telemetry.enabled = false;

  SECTION("header sent when apm_tracing_enabled is false") {
    config.tracing_enabled = false;
    auto finalized = finalize_config(config);
    REQUIRE(finalized);

    {
      http_client->response_status = 200;
      http_client->response_body << "{}";
      Tracer tracer{*finalized};
      Span span = tracer.create_span();
    }

    // the header is present
    const auto header_it = http_client->request_headers.items.find(
        "Datadog-Client-Computed-Stats");
    REQUIRE(header_it != http_client->request_headers.items.end());
    CHECK(header_it->second == "yes");
  }

  SECTION("header not sent when apm_tracing_enabled is true") {
    config.tracing_enabled = true;
    auto finalized = finalize_config(config);
    REQUIRE(finalized);

    {
      http_client->response_status = 200;
      http_client->response_body << "{}";
      Tracer tracer{*finalized};
      Span span = tracer.create_span();
    }

    // verify trhat the header is not present
    CHECK(http_client->request_headers.items.count(
              "Datadog-Client-Computed-Stats") == 0);
  }
}
