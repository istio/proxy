#include "collectors.h"

#include <datadog/json.hpp>

#define DEFINE_CONFIG_JSON_METHOD(TYPE)                      \
  std::string TYPE::config() const {                         \
    return nlohmann::json::object({{"type", #TYPE}}).dump(); \
  }

DEFINE_CONFIG_JSON_METHOD(MockCollector)
DEFINE_CONFIG_JSON_METHOD(MockCollectorWithResponse)
DEFINE_CONFIG_JSON_METHOD(PriorityCountingCollector)
DEFINE_CONFIG_JSON_METHOD(PriorityCountingCollectorWithResponse)
DEFINE_CONFIG_JSON_METHOD(FailureCollector)

#undef DEFINE_CONFIG_JSON_METHOD
