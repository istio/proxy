#include "collector_response.h"

namespace datadog {
namespace tracing {

std::string CollectorResponse::key(StringView service, StringView environment) {
  std::string result;
  result += "service:";
  append(result, service);
  result += ",env:";
  append(result, environment);
  return result;
}

const std::string CollectorResponse::key_of_default_rate =
    CollectorResponse::key("", "");

}  // namespace tracing
}  // namespace datadog
