// NOLINT(namespace-envoy)
#include <cstdlib>
#include <iostream>

#include "google/protobuf/descriptor.h"

// Basic C++ build/link validation for the v2 xDS APIs.
int main(int argc, char* argv[]) {
  const char* methods[] = {
      "xds.service.orca.v3.OpenRcaService.StreamCoreMetrics",
      // Old name for backward compatibility.
      // TODO(roth): Remove once all callers are updated to use the new name.
      "udpa.service.orca.v1.OpenRcaService.StreamCoreMetrics",
  };

  for (const char* method : methods) {
    if (google::protobuf::DescriptorPool::generated_pool()->FindMethodByName(method) == nullptr) {
      std::cout << "Unable to find method descriptor for " << method << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  exit(EXIT_SUCCESS);
}
