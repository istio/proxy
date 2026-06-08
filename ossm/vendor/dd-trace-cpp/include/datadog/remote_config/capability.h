#pragma once

#include <cstdint>

namespace datadog {
namespace remote_config {

// Type alias for capabilities flags.
//
// Usage:
//
// using namespace datadog::remote_config::capability;
// Capabilities c = APM_TRACING_SAMPLE_RATE | APM_TRACING_ENABLED;
using Capabilities = uint64_t;

namespace capability {

enum Flag : Capabilities {
  ASM_ACTIVATION = 1 << 1,
  ASM_IP_BLOCKING = 1 << 2,
  ASM_DD_RULES = 1 << 3,
  ASM_EXCLUSIONS = 1 << 4,
  ASM_REQUEST_BLOCKING = 1 << 5,
  ASM_RESPONSE_BLOCKING = 1 << 6,
  ASM_USER_BLOCKING = 1 << 7,
  ASM_CUSTOM_RULES = 1 << 8,
  ASM_CUSTOM_BLOCKING_RESPONSE = 1 << 9,
  ASM_TRUSTED_IPS = 1 << 10,
  ASM_API_SECURITY_SAMPLE_RATE = 1 << 11,
  APM_TRACING_SAMPLE_RATE = 1 << 12,
  APM_TRACING_LOGS_INJECTION = 1 << 13,
  APM_TRACING_HTTP_HEADER_TAGS = 1 << 14,
  APM_TRACING_TAGS = 1 << 15,
  ASM_PREPROCESSOR_OVERRIDES = 1 << 16,
  ASM_CUSTOM_DATA_SCANNERS = 1 << 17,
  ASM_EXCLUSION_DATA = 1 << 18,
  APM_TRACING_ENABLED = 1 << 19,
  APM_TRACING_DATA_STREAMS_ENABLED = 1 << 20,
  ASM_RASP_SQLI = 1 << 21,
  ASM_RASP_LFI = 1 << 22,
  ASM_RASP_SSRF = 1 << 23,
  ASM_RASP_SHI = 1 << 24,
  ASM_RASP_XXE = 1 << 25,
  ASM_RASP_RCE = 1 << 26,
  ASM_RASP_NOSQLI = 1 << 27,
  ASM_RASP_XSS = 1 << 28,
  APM_TRACING_SAMPLE_RULES = 1 << 29,
};

}  // namespace capability

}  // namespace remote_config
}  // namespace datadog
