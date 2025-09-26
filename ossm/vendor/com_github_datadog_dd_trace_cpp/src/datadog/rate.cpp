#include "rate.h"

#include <string>

namespace datadog {
namespace tracing {

Expected<Rate> Rate::from(double value) {
  if (value >= 0 && value <= 1) {
    return Rate(value);
  }

  std::string message;
  message +=
      "A rate must be no less than 0.0 and no more than 1.0, but we received: ";
  message += std::to_string(value);
  return Error{Error::RATE_OUT_OF_RANGE, std::move(message)};
}

}  // namespace tracing
}  // namespace datadog
