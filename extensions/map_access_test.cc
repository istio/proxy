#include "eval/eval/field_backed_map_impl.h"

#include <thread>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/synchronization/barrier.h"
#include "eval/testutil/test_message.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

constexpr const int kNumThreads = 200;

TEST(CelParserConcurrencyTest, ParseConcurrently) {
  TestMessage message;
  const std::string key = "test_key";
  auto field_map = message.mutable_string_int32_map();
  (*field_map)[key] = 1;

  std::vector<std::thread> threads(kNumThreads);
  auto barrier = std::make_unique<absl::Barrier>(kNumThreads);

  for (auto& thread : threads) {
    thread = std::thread([&barrier, &message, &key] {
      protobuf::Arena arena;
      const std::string other = "other_key";
      const protobuf::FieldDescriptor* field_desc =
        message.GetDescriptor()->FindFieldByName("string_int32_map");
      FieldBackedMapImpl cel_map(&message, field_desc, &arena);

      if (barrier->Block()) barrier.reset();

      EXPECT_EQ(cel_map[CelValue::CreateString(&key)]->Int64OrDie(), 1);
      EXPECT_FALSE(cel_map[CelValue::CreateString(&other)].has_value());
    });
  }
  for (std::thread& thread : threads) {
    thread.join();
  }
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
