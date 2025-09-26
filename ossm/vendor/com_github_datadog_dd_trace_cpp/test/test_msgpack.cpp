#include <datadog/error.h>
#include <datadog/msgpack.h>

#include <cstdint>
#include <string>
#include <utility>

#include "test.h"

namespace datadog {
namespace tracing {

bool operator==(const Error& left, const Error& right) {
  return left.code == right.code && left.message == right.message;
}

}  // namespace tracing
}  // namespace datadog

using namespace datadog::tracing;

TEST_CASE("array element fails to encode") {
  std::string destination;
  const int dummy[] = {42};
  const Error error{Error::OTHER, "any error will do"};
  const auto result = msgpack::pack_array(
      destination, dummy, [&](std::string&, int) { return error; });

  REQUIRE_FALSE(result);
  REQUIRE(result.error() == error);
}

TEST_CASE("map element fails to encode") {
  std::string destination;
  const Error error{Error::OTHER, "any error will do"};

  SECTION("sequence of pairs") {
    const std::pair<std::string, int> pairs[] = {
        std::pair<std::string, int>{"dummy", 42}};
    const auto result = msgpack::pack_map(
        destination, pairs, [&](std::string&, int) { return error; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == error);
  }

  SECTION("key/value arguments") {
    const auto succeed = [](std::string&) { return nullopt; };
    const auto fail = [&](std::string&) { return error; };
    const auto result =
        msgpack::pack_map(destination, "foo", succeed, "bar", fail);

    REQUIRE_FALSE(result);
    REQUIRE(result.error() == error);
  }
}

// The following group of tests verify that encoding routines return an error
// if the size of their input cannot fit in 32 bits.
// This is impossible to do on a 32-bit system, so these tests are excluded by
// the preprocessor when the pointer size is not greater than four.
#if UINTPTR_MAX > UINT32_MAX

// `OversizedSequence` can be used to test oversized inputs for both the
// sequence overloads of both the array and map encoding functions.
struct OversizedSequence {
  std::size_t size() const {
    return std::size_t(std::numeric_limits<std::uint32_t>::max()) + 1;
  }

  const std::pair<std::string, int>* begin() const { return nullptr; }
  const std::pair<std::string, int>* end() const { return nullptr; }
};

TEST_CASE("oversized string") {
  const char* const dummy = "doesn't matter";
  const auto oversized =
      std::size_t(std::numeric_limits<std::uint32_t>::max()) + 1;
  std::string destination;
  const auto result = msgpack::pack_string(destination, dummy, oversized);

  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == Error::MESSAGEPACK_ENCODE_FAILURE);
  REQUIRE(destination == "");
}

TEST_CASE("oversized array") {
  std::string destination;

  SECTION("just the header") {
    const auto oversized =
        std::size_t(std::numeric_limits<std::uint32_t>::max()) + 1;
    const auto result = msgpack::pack_array(destination, oversized);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == Error::MESSAGEPACK_ENCODE_FAILURE);
  }

  SECTION("sequence") {
    const auto result =
        msgpack::pack_array(destination, OversizedSequence{},
                            [](const auto&, const auto&) { return nullopt; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == Error::MESSAGEPACK_ENCODE_FAILURE);
  }

  REQUIRE(destination == "");
}

TEST_CASE("oversized map") {
  SECTION("just the header") {
    const auto oversized =
        std::size_t(std::numeric_limits<std::uint32_t>::max()) + 1;
    std::string destination;
    const auto result = msgpack::pack_map(destination, oversized);

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == Error::MESSAGEPACK_ENCODE_FAILURE);
    REQUIRE(destination == "");
  }

  SECTION("sequence of pairs") {
    std::string destination;
    const OversizedSequence oversized;
    const auto result =
        msgpack::pack_map(destination, oversized,
                          [](const auto&, const auto&) { return nullopt; });

    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == Error::MESSAGEPACK_ENCODE_FAILURE);
    REQUIRE(destination == "");
  }
}

#endif
