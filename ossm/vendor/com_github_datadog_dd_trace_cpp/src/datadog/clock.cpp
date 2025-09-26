#include "clock.h"

namespace datadog {
namespace tracing {

const Clock default_clock = []() {
  return TimePoint{std::chrono::system_clock::now(),
                   std::chrono::steady_clock::now()};
};

}  // namespace tracing
}  // namespace datadog
