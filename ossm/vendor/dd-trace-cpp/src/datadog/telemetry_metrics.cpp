#include "telemetry_metrics.h"

namespace datadog::tracing::metrics {

namespace tracer {
const telemetry::Counter spans_created = {"spans_created", "tracers", true};
const telemetry::Counter spans_dropped = {"spans_dropped", "tracers", true};
const telemetry::Counter spans_finished = {"spans_finished", "tracers", true};

const telemetry::Counter trace_segments_created = {"trace_segments_created",
                                                   "tracers", true};

const telemetry::Counter trace_segments_closed = {"trace_segments_closed",
                                                  "tracers", true};

const telemetry::Distribution trace_chunk_size = {"trace_chunk_size", "tracers",
                                                  true};

const telemetry::Distribution trace_chunk_serialized_bytes = {
    "trace_chunk_serialization.bytes", "tracers", true};

const telemetry::Distribution trace_chunk_serialization_duration = {
    "trace_chunk_serialization.ms", "tracers", true};

const telemetry::Counter trace_chunks_enqueued = {"trace_chunks_enqueued",
                                                  "tracers", true};

const telemetry::Counter trace_chunks_enqueued_for_serialization = {
    "trace_chunks_enqueued_for_serialization", "tracers", true};

const telemetry::Counter trace_chunks_dropped = {"trace_chunks_dropped",
                                                 "tracers", true};

const telemetry::Counter trace_chunks_sent = {"trace_chunks_sent", "tracers",
                                              true};

const telemetry::Counter context_header_truncated = {
    "context_header.truncated",
    "tracers",
    true,
};

namespace api {
const telemetry::Counter requests = {"trace_api.requests", "tracers", true};
const telemetry::Counter responses = {"trace_api.responses", "tracers", true};
const telemetry::Distribution bytes_sent = {"trace_api.bytes", "tracers", true};
const telemetry::Distribution request_duration = {"trace_api.ms", "tracers",
                                                  true};
const telemetry::Counter errors = {"trace_api.errors", "tracers", true};
}  // namespace api

namespace trace_context {
const telemetry::Counter injected = {"context_header_style.injected", "tracers",
                                     true};
const telemetry::Counter extracted = {"context_header_style.extracted",
                                      "tracers", true};
const telemetry::Counter truncated = {"context_header.truncated", "tracers",
                                      true};
const telemetry::Counter malformed = {"context_header_style.malformed",
                                      "tracers", true};
}  // namespace trace_context

}  // namespace tracer

}  // namespace datadog::tracing::metrics
