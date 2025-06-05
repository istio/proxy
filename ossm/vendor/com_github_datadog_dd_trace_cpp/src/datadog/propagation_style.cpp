#include "propagation_style.h"

#include <cassert>

#include "json.hpp"
#include "string_util.h"

namespace datadog {
namespace tracing {

StringView to_string_view(PropagationStyle style) {
  // Note: Make sure that these strings are consistent (modulo case) with
  // `parse_propagation_styles` in `tracer_config.cpp`.
  switch (style) {
    case PropagationStyle::DATADOG:
      return "Datadog";
    case PropagationStyle::B3:
      return "B3";
    case PropagationStyle::W3C:
      return "tracecontext";  // for compatibility with OpenTelemetry
    default:
      assert(style == PropagationStyle::NONE);
      return "none";
  }
}

nlohmann::json to_json(PropagationStyle style) { return to_string_view(style); }

nlohmann::json to_json(const std::vector<PropagationStyle>& styles) {
  std::vector<nlohmann::json> styles_json;
  for (const auto style : styles) {
    styles_json.push_back(to_json(style));
  }
  return styles_json;
}

Optional<PropagationStyle> parse_propagation_style(StringView text) {
  auto token = std::string{text};
  to_lower(token);

  // Note: Make sure that these strings are consistent (modulo case) with
  // `to_json`, above.
  if (token == "datadog") {
    return PropagationStyle::DATADOG;
  } else if (token == "b3" || token == "b3multi") {
    return PropagationStyle::B3;
  } else if (token == "tracecontext") {
    return PropagationStyle::W3C;
  } else if (token == "none") {
    return PropagationStyle::NONE;
  }

  return nullopt;
}

}  // namespace tracing
}  // namespace datadog
