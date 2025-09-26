#pragma once

// This component provides a functions that generate pseudo-random data.

#include <cstdint>
#include <string>

namespace datadog {
namespace tracing {

// Return a pseudo-random unsigned 64-bit integer. The sequence generated is
// thread-local and seeded randomly. The thread-local generator is reseeded when
// this process forks.
std::uint64_t random_uint64();

// Return a pseudo-random UUID in canonical string form as described in RFC
// 4122. For example, "595af0a4-ff29-4a8c-9f37-f8ff055e0f80".
std::string uuid();

}  // namespace tracing
}  // namespace datadog
