#pragma once

// This component provides platform-dependent miscellanea.

#include <string>

namespace datadog {
namespace tracing {

// Hold host information mainly used for telemetry purposes
// and for identifying a tracer.
struct HostInfo final {
  std::string os;
  std::string os_version;
  std::string hostname;
  std::string cpu_architecture;
  std::string kernel_name;
  std::string kernel_version;
  std::string kernel_release;
};

// Returns host information. Lazy.
HostInfo get_host_info();

std::string get_hostname();

int get_process_id();

int at_fork_in_child(void (*on_fork)());

}  // namespace tracing
}  // namespace datadog
