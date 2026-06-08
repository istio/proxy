#pragma once

#include <datadog/telemetry/metrics.h>

namespace datadog::tracing::metrics {

namespace tracer {

/// The number of spans created by the tracer, tagged by manual API
/// (`integration_name:datadog`, `integration_name:otel` or
/// `integration_name:opentracing`).
extern const telemetry::Counter spans_created;

/// The number of spans dropped and the reason for being dropped, for example
/// `reason:p0_drop` (the span was part of a p0 trace that was droped by the
/// tracer), `reason:overfull_buffer` (the local buffer was full, and the span
/// had to be dropped), `reason:serialization_error` (there was an error
/// serializing the span and it had to be dropped)
extern const telemetry::Counter spans_dropped;

/// The number of spans finished, optionally (if implementation allows) tagged
/// manual API (`integration_name:datadog`, `integration_name:otel` or
/// `integration_name:opentracing`).
extern const telemetry::Counter spans_finished;

/// The number of spans in the trace chunk when it is enqueued.
extern const telemetry::Distribution trace_chunk_size;

/// The size in bytes of the serialized trace chunk.
extern const telemetry::Distribution trace_chunk_serialized_bytes;

/// The time it takes to serialize a trace chunk.
extern const telemetry::Distribution trace_chunk_serialization_duration;

/// The number of times a trace chunk is enqueued for sampling/serialization. In
/// partial-flush scenarios, multiple trace chunks may be enqueued per trace
/// segment/local trace.
extern const telemetry::Counter trace_chunks_enqueued;

/// The number of trace chunks kept for serialization. Excludes single-span
/// sampling spans. Tagged by one of `reason:p0_keep` (the trace was a p0 trace
/// that was kept for sending to the agent) or `reason:default` - The tracer is
/// not dropping p0 spans, so the span was enqueued 'by default' for sending to
/// the trace-agent).
extern const telemetry::Counter trace_chunks_enqueued_for_serialization;

/// The number of trace chunks dropped prior to serialization, tagged by reason.
/// Includes traces which are dropped due to errors, overfull buffers, as well
/// as due to sampling decision. For example `reason:p0_drop` (the span a p0
/// trace that was droped by the tracer), `reason:overfull_buffer` (the local
/// buffer was full, and the trace chunk had to be dropped),
/// `reason:serialization_error` (there was an error serializing the trace and
/// it had to be dropped).
extern const telemetry::Counter trace_chunks_dropped;

/// The number of trace chunks attempted to be sent to the backend, regardless
/// of response.
extern const telemetry::Counter trace_chunks_sent;

/// The number of trace segments (local traces) created, tagged with
/// new/continued depending on whether this is a new trace (no distributed
/// context information) or continued (has distributed context).
extern const telemetry::Counter trace_segments_created;

/// The number of trace segments (local traces) closed. In non partial flush
/// scenarios, trace_segments_closed == trace_chunks_enqueued.
extern const telemetry::Counter trace_segments_closed;

namespace api {

/// The number of requests sent to the trace endpoint in the agent, regardless
/// of success.
extern const telemetry::Counter requests;

/// The number of responses received from the trace endpoint, tagged with status
/// code, e.g. `status_code:200`, `status_code:404`. May also use
/// `status_code:5xx` for example as a catch-all for 2xx, 3xx, 4xx, 5xx
/// responses.
extern const telemetry::Counter responses;

/// The size of the payload sent to the endpoint in bytes.
extern const telemetry::Distribution bytes_sent;

/// The time it takes to flush the trace payload to the agent. Note that this is
/// not the per trace time, this is the per payload time.
extern const telemetry::Distribution request_duration;

/// The number of requests sent to the trace endpoint in the agent that errored,
/// tagged by the error type (e.g. `type:timeout`, `type:network`,
/// `type:status_code`).
extern const telemetry::Counter errors;

}  // namespace api

namespace trace_context {

/// The number of times distributed context is injected into an outgoing span,
/// tagged by header style (`header_style:tracecontext`, `header_style:datadog`,
/// `header_style:b3multi`, `header_style:b3single`, `header_style:baggage`)
extern const telemetry::Counter injected;

/// The number of times distributed context is successfully extracted from an
/// outgoing span, tagged by header style (`header_style:tracecontext`,
/// `header_style:datadog`, `header_style:b3multi`, `header_style:b3single`,
/// `header_style:baggage`)
extern const telemetry::Counter extracted;

/// The number of times a context propagation header is truncated, tagged by the
/// reason for truncation (`truncation_reason:baggage_item_count_exceeded`,
/// `truncation_reason:baggage_byte_count_exceeded`)
extern const telemetry::Counter truncated;

/// The number of times baggage headers are dropped because they're malformed
/// (missing key/value/'='), tagged by header style (`header_style:baggage`)
extern const telemetry::Counter malformed;
}  // namespace trace_context

}  // namespace tracer

}  // namespace datadog::tracing::metrics
