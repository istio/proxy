#include "extensions/common/util.h"

#include "gtest/gtest.h"

namespace Wasm {
namespace Common {
namespace {

TEST(WasmCommonUtilsTest, ParseResponseFlag) {
  std::vector<std::pair<uint64_t, std::string>> expected = {
      std::make_pair(0x1, "LH"),      std::make_pair(0x2, "UH"),
      std::make_pair(0x4, "UT"),      std::make_pair(0x8, "LR"),
      std::make_pair(0x10, "UR"),     std::make_pair(0x20, "UF"),
      std::make_pair(0x40, "UC"),     std::make_pair(0x80, "UO"),
      std::make_pair(0x100, "NR"),    std::make_pair(0x200, "DI"),
      std::make_pair(0x400, "FI"),    std::make_pair(0x800, "RL"),
      std::make_pair(0x1000, "UAEX"), std::make_pair(0x2000, "RLSE"),
      std::make_pair(0x4000, "DC"),   std::make_pair(0x8000, "URX"),
      std::make_pair(0x10000, "SI"),  std::make_pair(0x20000, "IH"),
      std::make_pair(0x40000, "DPE"),
  };

  for (const auto& test_case : expected) {
    EXPECT_EQ(test_case.second, parseResponseFlag(test_case.first));
  }

  // No flag is set.
  { EXPECT_EQ("-", parseResponseFlag(0x0)); }

  // Test combinations.
  // These are not real use cases, but are used to cover multiple response flags
  // case.
  { EXPECT_EQ("UT,DI,FI", parseResponseFlag(0x604)); }

  // Test overflow.
  { EXPECT_EQ("DPE,786432", parseResponseFlag(0xC0000)); }
}

}  // namespace
}  // namespace Common
}  // namespace Wasm
