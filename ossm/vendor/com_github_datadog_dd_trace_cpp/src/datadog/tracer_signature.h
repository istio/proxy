#pragma once

// This component provides a class, `TracerSignature`, that contains the parts
// of a tracer's configuration that are used to refer to the tracer in Datadog's
// telemetry and remote configuration APIs.
//
// `TracerSignature` is used in three contexts:
//
// 1. When telemetry is sent to the Datadog Agent, the tracer signature is
//    included in the request payload. See
//    `TracerTelemetry::generate_telemetry_body` in `tracer_telemetry.cpp`.
// 2. When the Datadog Agent is polled for configuration updates, part of the
//    tracer signature (all but the language version) is included in the request
//    payload. See `RemoteConfigurationManager::make_request_payload` in
//    `remote_config.h`.
// 3. When the Datadog Agent responds with configuration updates, the service
//    and environment of the tracer signature are used to determine whether the
//    updates are relevant to the `Tracer` that created the collector that is
//    polling the Datadog Agent. See
//    `RemoteConfigurationManager::process_response` in `remote_config.h`.

#include <string>

#include "runtime_id.h"
#include "string_view.h"
#include "version.h"

#define DD_TRACE_STRINGIFY(ARG) DD_TRACE_STRINGIFY_HELPER(ARG)
#define DD_TRACE_STRINGIFY_HELPER(ARG) #ARG

namespace datadog {
namespace tracing {

struct TracerSignature {
  RuntimeID runtime_id;
  std::string default_service;
  std::string default_environment;
  std::string library_version;
  StringView library_language;
  StringView library_language_version;

  TracerSignature() = delete;
  TracerSignature(RuntimeID id, std::string service, std::string environment)
      : runtime_id(id),
        default_service(std::move(service)),
        default_environment(std::move(environment)),
        library_version(tracer_version),
        library_language("cpp"),
        library_language_version(DD_TRACE_STRINGIFY(__cplusplus), 6) {}
};

}  // namespace tracing
}  // namespace datadog

#undef DD_TRACE_STRINGIFY_HELPER
#undef DD_TRACE_STRINGIFY
