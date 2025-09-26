#pragma once

// This component provides facilities for extracting trace context from a
// `DictReader`. It is used by `Tracer::extract_trace`. See `tracer.cpp`.

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "dict_reader.h"
#include "expected.h"
#include "optional.h"
#include "propagation_style.h"

namespace datadog {
namespace tracing {

struct ExtractedData;
class Logger;

// Parse the high 64 bits of a trace ID from the specified `value`. If `value`
// is correctly formatted, then return the resulting bits. If `value` is
// incorrectly formatted, then return `nullopt`.
Optional<std::uint64_t> parse_trace_id_high(const std::string& value);

// Return trace information parsed from the specified `headers` in the Datadog
// propagation style. Use the specified `span_tags` and `logger` to report
// warnings. If an error occurs, return an `Error`.
Expected<ExtractedData> extract_datadog(
    const DictReader& headers,
    std::unordered_map<std::string, std::string>& span_tags, Logger& logger);

// Return trace information parsed from the specified `headers` in the B3
// multi-header propagation style. If an error occurs, return an `Error`.
Expected<ExtractedData> extract_b3(
    const DictReader& headers, std::unordered_map<std::string, std::string>&,
    Logger&);

// Return an `ExtractedData` whose only non-default field is
// `style = PropagationStyle::NONE`.
Expected<ExtractedData> extract_none(
    const DictReader&, std::unordered_map<std::string, std::string>&, Logger&);

// Return a string that can be used as the argument to `Error::with_prefix` for
// errors occurring while extracting trace information in the specified `style`
// from the specified `headers_examined`.
std::string extraction_error_prefix(
    const Optional<PropagationStyle>& style,
    const std::vector<std::pair<std::string, std::string>>& headers_examined);

// `AuditedReader` is a `DictReader` that remembers all key/value pairs looked
// up or visited through it. It remembers a lookup only if it yielded a non-null
// value. This is used for error diagnostic messages in trace extraction (i.e.
// an error occurred, but which HTTP request headers were we looking at?).
struct AuditedReader : public DictReader {
  const DictReader& underlying;
  mutable std::vector<std::pair<std::string, std::string>> entries_found;

  explicit AuditedReader(const DictReader& underlying);

  Optional<StringView> lookup(StringView key) const override;

  void visit(const std::function<void(StringView key, StringView value)>&
                 visitor) const override;
};

// Combine the specified trace `contexts`, each of which was extracted in a
// particular propagation style, into one `ExtractedData` that includes fields
// from compatible elements of `contexts`, and return the resulting
// `ExtractedData`. The order of the elements of `contexts` must correspond to
// the order of the configured extraction propagation styles.
ExtractedData merge(const std::vector<ExtractedData>& contexts);

}  // namespace tracing
}  // namespace datadog
