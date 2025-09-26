#pragma once

// This component provides functions for extracting and injecting trace context
// in the `PropagationStyle::W3C` style. These functions decode and encode the
// "traceparent" and "tracestate" HTTP request headers.

#include <cstdint>
#include <string>
#include <unordered_map>

#include "expected.h"
#include "extracted_data.h"
#include "optional.h"
#include "trace_id.h"

namespace datadog {
namespace tracing {

class DictReader;
class Logger;

// Return `ExtractedData` deduced from the "traceparent" and "tracestate"
// entries of the specified `headers`. If an error occurs, set a value for the
// `tags::internal::w3c_extraction_error` tag in the specified `span_tags`.
// `extract_w3c` will not return an error; instead, it returns an empty
// `ExtractedData` when extraction fails.
Expected<ExtractedData> extract_w3c(
    const DictReader& headers,
    std::unordered_map<std::string, std::string>& span_tags, Logger&);

// Return a value for the "traceparent" header consisting of the specified
// `trace_id` or the optionally specified `full_w3c_trace_id_hex` as the trace
// ID, the specified `span_id` as the parent ID, and trace flags deduced from
// the specified `sampling_priority`.
std::string encode_traceparent(TraceID trace_id, std::uint64_t span_id,
                               int sampling_priority);

// Return a value for the "tracestate" header containing the specified fields.
std::string encode_tracestate(
    uint64_t span_id, int sampling_priority,
    const Optional<std::string>& origin,
    const std::vector<std::pair<std::string, std::string>>& trace_tags,
    const Optional<std::string>& additional_datadog_w3c_tracestate,
    const Optional<std::string>& additional_w3c_tracestate);

}  // namespace tracing
}  // namespace datadog
