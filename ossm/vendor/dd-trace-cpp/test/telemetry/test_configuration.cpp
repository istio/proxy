#include <datadog/environment.h>
#include <datadog/telemetry/configuration.h>

#include "../common/environment.h"
#include "../test.h"

namespace ddtest = datadog::test;

using namespace datadog;
using namespace std::literals;

#define TELEMETRY_CONFIGURATION_TEST(x) \
  TEST_CASE(x, "[telemetry],[telemetry.configuration]")

TELEMETRY_CONFIGURATION_TEST("defaults") {
  const auto cfg = telemetry::finalize_config();
  REQUIRE(cfg);
  CHECK(cfg->debug == false);
  CHECK(cfg->enabled == true);
  CHECK(cfg->report_logs == true);
  CHECK(cfg->report_metrics == true);
  CHECK(cfg->metrics_interval == 60s);
  CHECK(cfg->heartbeat_interval == 10s);
  CHECK(cfg->install_id.has_value() == false);
  CHECK(cfg->install_type.has_value() == false);
  CHECK(cfg->install_time.has_value() == false);
}

TELEMETRY_CONFIGURATION_TEST("code override") {
  telemetry::Configuration cfg;
  cfg.enabled = false;
  cfg.report_logs = false;
  cfg.report_metrics = false;
  cfg.metrics_interval_seconds = 1;
  cfg.heartbeat_interval_seconds = 2;
  cfg.integration_name = "test";
  cfg.integration_version = "2024.10.28";

  auto final_cfg = finalize_config(cfg);
  REQUIRE(final_cfg);
  CHECK(final_cfg->enabled == false);
  CHECK(final_cfg->debug == false);
  CHECK(final_cfg->report_logs == false);
  CHECK(final_cfg->report_metrics == false);
  CHECK(final_cfg->metrics_interval == 1s);
  CHECK(final_cfg->heartbeat_interval == 2s);
  CHECK(final_cfg->integration_name == "test");
  CHECK(final_cfg->integration_version == "2024.10.28");
}

TELEMETRY_CONFIGURATION_TEST("enabled and report metrics precedence") {
  SECTION("enabled takes precedence over metrics enabled") {
    telemetry::Configuration cfg;
    cfg.enabled = false;
    cfg.report_logs = true;
    cfg.report_metrics = true;

    auto final_cfg = finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->enabled == false);
    CHECK(final_cfg->report_logs == false);
    CHECK(final_cfg->report_metrics == false);
  }
}

TELEMETRY_CONFIGURATION_TEST("environment environment override") {
  telemetry::Configuration cfg;

  SECTION("Override `enabled` field") {
    cfg.enabled = true;
    ddtest::EnvGuard env("DD_INSTRUMENTATION_TELEMETRY_ENABLED", "false");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->enabled == false);
  }

  SECTION("Override `debug` field") {
    cfg.enabled = true;
    ddtest::EnvGuard env("DD_TELEMETRY_DEBUG", "true");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->debug == true);
  }

  SECTION("Override `report_metrics` field") {
    cfg.report_metrics = true;
    ddtest::EnvGuard env("DD_TELEMETRY_METRICS_ENABLED", "false");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->report_metrics == false);
  }

  SECTION("Override `report_logs` field") {
    cfg.report_logs = true;
    ddtest::EnvGuard env("DD_TELEMETRY_LOG_COLLECTION_ENABLED", "false");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->report_logs == false);
  }

  SECTION("Override metrics interval") {
    cfg.metrics_interval_seconds = 88;
    ddtest::EnvGuard env("DD_TELEMETRY_METRICS_INTERVAL_SECONDS", "15");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->metrics_interval == 15s);
  }

  SECTION("Override heartbeat interval") {
    cfg.heartbeat_interval_seconds = 61;
    ddtest::EnvGuard env("DD_TELEMETRY_HEARTBEAT_INTERVAL", "42");
    auto final_cfg = telemetry::finalize_config(cfg);
    REQUIRE(final_cfg);
    CHECK(final_cfg->heartbeat_interval == 42s);
  }
}

TELEMETRY_CONFIGURATION_TEST("validation") {
  SECTION("metrics interval validation") {
    SECTION("code override") {
      telemetry::Configuration cfg;
      cfg.metrics_interval_seconds = -15;

      auto final_cfg = telemetry::finalize_config(cfg);
      REQUIRE(!final_cfg);
    }

    SECTION("environment variable override") {
      ddtest::EnvGuard env("DD_TELEMETRY_METRICS_INTERVAL_SECONDS", "-18");
      auto final_cfg = telemetry::finalize_config();
      REQUIRE(!final_cfg);
    }
  }

  SECTION("heartbeat interval validation") {
    SECTION("code override") {
      telemetry::Configuration cfg;
      cfg.heartbeat_interval_seconds = -30;

      auto final_cfg = telemetry::finalize_config(cfg);
      REQUIRE(!final_cfg);
    }

    SECTION("environment variable override") {
      ddtest::EnvGuard env("DD_TELEMETRY_METRICS_INTERVAL_SECONDS", "-42");
      auto final_cfg = telemetry::finalize_config();
      REQUIRE(!final_cfg);
    }
  }
}

TELEMETRY_CONFIGURATION_TEST("installation infos are used when available") {
  ddtest::EnvGuard install_id_env("DD_INSTRUMENTATION_INSTALL_ID", "1-2-3-4");
  ddtest::EnvGuard install_type_env("DD_INSTRUMENTATION_INSTALL_TYPE", "ssi");
  ddtest::EnvGuard install_time_env("DD_INSTRUMENTATION_INSTALL_TIME", "now");

  const auto final_cfg = telemetry::finalize_config();
  REQUIRE(final_cfg);

  REQUIRE(final_cfg->install_id.has_value());
  CHECK(final_cfg->install_id.value() == "1-2-3-4");

  REQUIRE(final_cfg->install_type.has_value());
  CHECK(final_cfg->install_type.value() == "ssi");

  REQUIRE(final_cfg->install_time.has_value());
  CHECK(final_cfg->install_time.value() == "now");
}
