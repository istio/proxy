#include "runtime_id.h"

#include "random.h"

namespace datadog {
namespace tracing {

RuntimeID::RuntimeID() {}

RuntimeID RuntimeID::generate() {
  RuntimeID id;
  id.uuid_ = uuid();
  return id;
}

}  // namespace tracing
}  // namespace datadog
