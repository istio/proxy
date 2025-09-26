#include "error.h"

#include <ostream>
#include <sstream>

namespace datadog {
namespace tracing {

std::ostream& operator<<(std::ostream& stream, const Error& error) {
  return stream << "[dd-trace-cpp error code " << int(error.code) << "] "
                << error.message;
}

Error Error::with_prefix(StringView prefix) const {
  std::string new_message{prefix.begin(), prefix.end()};
  new_message += message;
  return Error{code, std::move(new_message)};
}

}  // namespace tracing
}  // namespace datadog
