#include "null_collector.h"

#include "json.hpp"

namespace datadog {
namespace tracing {

nlohmann::json NullCollector::config_json() const {
  // clang-format off
    return nlohmann::json::object({
        {"type", "datadog::tracing::NullCollector"},
        {"config", nlohmann::json::object({})},
    });
  // clang-format on
}

}  // namespace tracing
}  // namespace datadog
