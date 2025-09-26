#include "random.h"

#include <bitset>
#include <random>

#include "hex.h"
#include "platform_util.h"

namespace datadog {
namespace tracing {
namespace {

extern "C" void on_fork();

class Uint64Generator {
  std::mt19937_64 generator_;
  std::uniform_int_distribution<std::uint64_t> distribution_;

 public:
  Uint64Generator() {
    seed_with_random();
    // If a process links to this library and then calls `fork`, the
    // `generator_` in the parent and child processes will produce the exact
    // same sequence of values, which is bad.
    // A subsequent call to `exec` would remedy this, but nginx in particular
    // does not call `exec` after forking its worker processes.
    // So, we use `at_fork_in_child` to re-seed `generator_` in the child
    // process after `fork`.
    (void)at_fork_in_child(&on_fork);
  }

  std::uint64_t operator()() { return distribution_(generator_); }

  void seed_with_random() { generator_.seed(std::random_device{}()); }
};

thread_local Uint64Generator thread_local_generator;

void on_fork() { thread_local_generator.seed_with_random(); }

}  // namespace

std::uint64_t random_uint64() { return thread_local_generator(); }

std::string uuid() {
  // clang-format off
  // It's not all random.  From most significant to least significant, the
  // bits look like this:
  //
  //     xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx 0100xxxx xxxxxxxx 10xxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx
  //
  // where the "0" and "1" are hard-coded bits, and all of the "x" are random.
  // See RFC 4122 for more information.
  // clang-format on
  std::bitset<64> high = random_uint64();
  std::bitset<64> low = random_uint64();

  // Set "0100" for the most significant bits of the
  // second-to-least-significant byte of `high`.
  high[15] = 0;
  high[14] = 1;
  high[13] = 0;
  high[12] = 0;

  // Set "10" for the most significant bits of `low`.
  low[63] = 1;
  low[62] = 0;

  std::string result;
  std::string hexed = hex_padded(high.to_ullong());
  result += hexed.substr(0, 8);
  result += '-';
  result += hexed.substr(8, 4);
  result += '-';
  result += hexed.substr(12);
  result += '-';
  hexed = hex_padded(low.to_ullong());
  result += hexed.substr(0, 4);
  result += '-';
  result += hexed.substr(4);

  return result;
}

}  // namespace tracing
}  // namespace datadog
