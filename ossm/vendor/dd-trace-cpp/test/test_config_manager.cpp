#include <iostream>

#include "catch.hpp"
#include "datadog/config_manager.h"
#include "datadog/remote_config/listener.h"
#include "datadog/trace_sampler.h"
#include "mocks/http_clients.h"

namespace rc = datadog::remote_config;

using namespace datadog;
using namespace datadog::tracing;

#define CONFIG_MANAGER_TEST(x) TEST_CASE(x, "[config_manager]")

nlohmann::json load_json(std::string_view sv) {
  auto j = nlohmann::json::parse(/* input = */ sv,
                                 /* parser_callback = */ nullptr,
                                 /* allow_exceptions = */ false);
  REQUIRE(!j.is_discarded());
  return j;
}

CONFIG_MANAGER_TEST("remote configuration handling") {
  TracerConfig config;
  config.service = "testsvc";
  config.environment = "test";

  auto final_cfg = *finalize_config(config);

  auto http_client = std::make_shared<MockHTTPClient>();

  // TODO: set mock telemetry
  ConfigManager config_manager(final_cfg);

  rc::Listener::Configuration config_update{/* id = */ "id",
                                            /* path = */ "",
                                            /* content = */ "",
                                            /* version = */ 1,
                                            rc::product::Flag::APM_TRACING};

  SECTION("handling of `tracing_sampling_rate`") {
    SECTION("assess field validation") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "rate outside of [0;1] range 1/2",
              R"("tracing_sampling_rate": 100)",
          },
          {
              __LINE__,
              "rate outside of [0;1] range 2/2",
              R"("tracing_sampling_rate": -0.2)",
          },
          {
              __LINE__,
              "not a number 1/2",
              R"("tracing_sampling_rate": "quarante-deux")",
          },
          {
              __LINE__,
              "not a number 2/2",
              R"("tracing_sampling_rate": true)",
          },
          {
              __LINE__,
              "not a number 2/2",
              R"("tracing_sampling_rate": {"value": 0.5})",
          },
      }));

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          %s
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      const auto old_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      const auto err = config_manager.on_update(config_update);
      CHECK(err);

      const auto new_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      CHECK(old_trace_sampler_config == new_trace_sampler_config);
    }

    SECTION(
        "an RC payload without the `tracing_sampling_rate` or with a null "
        "value does not raise an error nor update the sampling rate") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "tracing_sampling_rate is missing",
              "",
          },
          {
              __LINE__,
              "tracing_sampling_rate is null",
              R"("tracing_sampling_rate": null,)",
          },
      }));

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          %s
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test"
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      const auto old_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      CHECK(old_trace_sampler_config == new_trace_sampler_config);
    }

    SECTION(
        "A valid RC payload update the sampling rules and reverting restore "
        "the initial value") {
      config_update.content = R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          "tracing_sampling_rate": 0.6
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })";

      const auto old_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      CHECK(old_trace_sampler_config != new_trace_sampler_config);

      config_manager.on_revert(config_update);

      const auto revert_trace_sampler_config =
          config_manager.trace_sampler()->config_json();

      CHECK(old_trace_sampler_config == revert_trace_sampler_config);
    }
  }

  SECTION("handling of `tracing_tags`") {
    SECTION("assess field validation returns an error") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "not an array",
              R"("tracing_tags": 15)",
          },
          {
              __LINE__,
              "not an array",
              R"("tracing_tags": "foo")",
          },
          {
              __LINE__,
              "not an array",
              R"("tracing_tags": {"key": "a", "value": "b"})",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          %s
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_tags = config_manager.span_defaults()->tags;

      const auto err = config_manager.on_update(config_update);
      CHECK(err);

      const auto new_tags = config_manager.span_defaults()->tags;

      // Make sure tags are not updated
      CHECK(old_tags == new_tags);
    }

    SECTION(
        "payload without `tracing_tags` or with a null value does not raise an "
        "error nor update the "
        "list of tags") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "tracing_tags is missing",
              "",
          },
          {
              __LINE__,
              "tracing_tags is null",
              R"("tracing_tags": null,)",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          %s
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test"
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_tags = config_manager.span_defaults()->tags;

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_tags = config_manager.span_defaults()->tags;

      // Make sure tags are not updated
      CHECK(old_tags == new_tags);
    }

    SECTION(
        "A valid RC payload does update the list of tags and reverting restore "
        "the initial list of tags") {
      config_update.content = R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          "tracing_tags": [
             "hello:world",
             "foo:bar"
          ]
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })";

      const std::unordered_map<std::string, std::string> expected_tags{
          {"hello", "world"}, {"foo", "bar"}};

      const auto old_tags = config_manager.span_defaults()->tags;

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_tags = config_manager.span_defaults()->tags;

      CHECK(old_tags != new_tags);
      CHECK(new_tags == expected_tags);

      config_manager.on_revert(config_update);

      const auto reverted_tags = config_manager.span_defaults()->tags;

      CHECK(old_tags == reverted_tags);
    }
  }

  SECTION("handling of `tracing_enabled`") {
    SECTION("validation") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "not a boolean",
              R"("tracing_enabled": "false")",
          },
          {
              __LINE__,
              "not a boolean 2/x",
              R"("tracing_enabled": ["false"])",
          },
          {
              __LINE__,
              "not a boolean 2/x",
              R"("tracing_enabled": 26)",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          %s
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_tracing_status = config_manager.report_traces();

      const auto err = config_manager.on_update(config_update);
      CHECK(err);

      const auto new_tracing_status = config_manager.report_traces();

      CHECK(old_tracing_status == new_tracing_status);
    }

    SECTION(
        "An RC payload without `tracing_enabled` or with a null value does not "
        "raise an error nor update the value") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "tracing_enabled is absent from the RC payload",
              "",
          },
          {
              __LINE__,
              "tracing_enabled is null",
              R"("tracing_enabled": null,)",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          %s
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test"
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_tracing_status = config_manager.report_traces();

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_tracing_status = config_manager.report_traces();

      CHECK(old_tracing_status == new_tracing_status);
    }

    SECTION("valid") {
      config_update.content = R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          "tracing_enabled": false,
          "tracing_sampling_rate": 0.6,
          "tracing_tags": [
             "hello:world",
             "foo:bar"
          ]
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })";

      const auto old_tracing_status = config_manager.report_traces();

      const auto err = config_manager.on_update(config_update);
      CHECK(!err);

      const auto new_tracing_status = config_manager.report_traces();

      CHECK(old_tracing_status != new_tracing_status);
      CHECK(new_tracing_status == false);

      config_manager.on_revert(config_update);

      const auto reverted_tracing_status = config_manager.report_traces();
      CHECK(old_tracing_status == reverted_tracing_status);
    }
  }

  SECTION("handling of `tracing_sampling_rules`") {
    SECTION("validation") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "not an array",
              R"("tracing_sampling_rules": "service:a,sample_rate:12")",
          },
          {
              __LINE__,
              "not an array 2/x",
              R"("tracing_sampling_rules": 28)",
          },
          {
              __LINE__,
              "not a valid sampling rules 1/x",
              R"("tracing_sampling_rules": ["foo", "bar"])",
          },
          {
              __LINE__,
              "missing required fields 1/x",
              R"("tracing_sampling_rules": [{"foo": "bar"}])",
          },
          {
              __LINE__,
              "missing required fields 2/x",
              R"("tracing_sampling_rules": [{"service": "bar"}])",
          },
          {
              __LINE__,
              "missing required fields 3/x",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo"}])",
          },
          {
              __LINE__,
              "missing required fields 3/x",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo", "sample_rate": 0.2}])",
          },
          {
              __LINE__,
              "invalid value for `service` field",
              R"("tracing_sampling_rules": [{"service": ["a", "b"], "resource": "yo", "sample_rate": 0.2, "provenance": "customer"}])",
          },
          {
              __LINE__,
              "invalid value for `resource` field",
              R"("tracing_sampling_rules": [{"service": "a", "resource": true, "sample_rate": 0.2, "provenance": "customer"}])",
          },
          {
              __LINE__,
              "invalid value for `provenance` field",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo", "sample_rate": 0.2, "provenance": "ui"}])",
          },
          {
              __LINE__,
              "invalid value for `sample_rate` field",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo", "sample_rate": "0.5", "provenance": "customer"}])",
          },
          {
              __LINE__,
              "invalid value for `tags` field",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo", "sample_rate": 0.2, "provenance": "customer", "tags": "tag1"}])",
          },
          {
              __LINE__,
              "invalid second rules",
              R"("tracing_sampling_rules": [{"service": "bar", "resource": "yo", "sample_rate": 0.2, "provenance": "customer"}, {"foo": "bar"}])",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          %s
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_sampler_cfg =
          config_manager.trace_sampler()->config_json();
      const auto err = config_manager.on_update(config_update);
      CHECK(err);
      const auto new_sampler_cfg =
          config_manager.trace_sampler()->config_json();

      CHECK(old_sampler_cfg == new_sampler_cfg);
    }

    SECTION("null value or the absence of the field is ignored") {
      struct TestCase {
        size_t line;
        std::string_view name;
        std::string_view input;
      };

      const auto test_case = GENERATE(values<TestCase>({
          {
              __LINE__,
              "tracing_sampling_rules is absent from the RC payload",
              "",
          },
          {
              __LINE__,
              "tracing_sampling_rules is null",
              R"("tracing_sampling_rules": null,)",
          },
      }));

      CAPTURE(test_case.line);
      CAPTURE(test_case.name);

      char payload[1024];
      std::snprintf(payload, 1024, R"({
        "lib_config": {
          %s
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test"
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })",
                    test_case.input.data());

      config_update.content = payload;

      const auto old_sampler_cfg =
          config_manager.trace_sampler()->config_json();
      const auto err = config_manager.on_update(config_update);
      CHECK(!err);
      const auto new_sampler_cfg =
          config_manager.trace_sampler()->config_json();

      CHECK(old_sampler_cfg == new_sampler_cfg);
    }

    SECTION("valid input") {
      config_update.content = R"({
        "lib_config": {
          "library_language": "all",
          "library_version": "latest",
          "service_name": "testsvc",
          "env": "test",
          "tracing_sampling_rules": [
            {
              "service": "foo",
              "resource": "GET /hello",
              "sample_rate": 0.1,
              "provenance": "customer",
              "name": "test",
              "tags": [
                { "key": "tag1", "value_glob": "value1" }
              ]
            }
          ]
        },
        "service_target": {
           "service": "testsvc",
           "env": "test"
        }
      })";

      const auto old_sampler_cfg =
          config_manager.trace_sampler()->config_json();
      const auto err = config_manager.on_update(config_update);
      CHECK(!err);
      const auto new_sampler_cfg =
          config_manager.trace_sampler()->config_json();

      CHECK(old_sampler_cfg != new_sampler_cfg);

      config_manager.on_revert({});

      const auto reverted_sampler_cfg =
          config_manager.trace_sampler()->config_json();

      CHECK(old_sampler_cfg == reverted_sampler_cfg);
    }
  }
}
