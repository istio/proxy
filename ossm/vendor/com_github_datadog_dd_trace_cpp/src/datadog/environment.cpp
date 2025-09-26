#include "environment.h"

#include <cstdlib>

#include "json.hpp"

namespace datadog {
namespace tracing {
namespace environment {

StringView name(Variable variable) { return variable_names[variable]; }

Optional<StringView> lookup(Variable variable) {
  const char *name = variable_names[variable];
  const char *value = std::getenv(name);
  if (!value) {
    return nullopt;
  }
  return StringView{value};
}

nlohmann::json to_json() {
  auto result = nlohmann::json::object({});

  for (const char *name : variable_names) {
    if (const char *value = std::getenv(name)) {
      result[name] = value;
    }
  }

  return result;
}

}  // namespace environment
}  // namespace tracing
}  // namespace datadog
