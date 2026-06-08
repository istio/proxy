#include "eval/public/cel_type_registry.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/type_provider.h"
#include "common/memory.h"
#include "common/type.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::MemoryManagerRef;
using ::cel::Type;
using ::cel::TypeProvider;
using ::testing::Contains;
using ::testing::Key;
using ::testing::Optional;

class TestTypeProvider : public LegacyTypeProvider {
 public:
  explicit TestTypeProvider(std::vector<std::string> types)
      : types_(std::move(types)) {}

  // Return a type adapter for an opaque type
  // (no reflection operations supported).
  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    for (const auto& type : types_) {
      if (name == type) {
        return LegacyTypeAdapter(/*access=*/nullptr, /*mutation=*/nullptr);
      }
    }
    return absl::nullopt;
  }

 private:
  std::vector<std::string> types_;
};

TEST(CelTypeRegistryTest, RegisterEnum) {
  CelTypeRegistry registry;
  registry.RegisterEnum("google.api.expr.runtime.TestMessage.TestEnum",
                        {
                            {"TEST_ENUM_UNSPECIFIED", 0},
                            {"TEST_ENUM_1", 10},
                            {"TEST_ENUM_2", 20},
                            {"TEST_ENUM_3", 30},
                        });

  EXPECT_THAT(registry.resolveable_enums(),
              Contains(Key("google.api.expr.runtime.TestMessage.TestEnum")));
}

TEST(CelTypeRegistryTest, TestRegisterBuiltInEnum) {
  CelTypeRegistry registry;

  ASSERT_THAT(registry.resolveable_enums(),
              Contains(Key("google.protobuf.NullValue")));
}

TEST(CelTypeRegistryTest, TestGetFirstTypeProviderSuccess) {
  CelTypeRegistry registry;
  auto type_provider = registry.GetFirstTypeProvider();
  ASSERT_NE(type_provider, nullptr);
  ASSERT_FALSE(
      type_provider->ProvideLegacyType("google.protobuf.Int64").has_value());
  ASSERT_TRUE(
      type_provider->ProvideLegacyType("google.protobuf.Any").has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFound) {
  CelTypeRegistry registry;
  auto desc = registry.FindTypeAdapter("google.protobuf.Any");
  ASSERT_TRUE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFoundMultipleProviders) {
  CelTypeRegistry registry;
  auto desc = registry.FindTypeAdapter("google.protobuf.Any");
  ASSERT_TRUE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterNotFound) {
  CelTypeRegistry registry;
  auto desc = registry.FindTypeAdapter("missing.MessageType");
  EXPECT_FALSE(desc.has_value());
}

MATCHER_P(TypeNameIs, name, "") {
  const Type& type = arg;
  *result_listener << "got typename: " << type.name();
  return type.name() == name;
}

TEST(CelTypeRegistryTypeProviderTest, Builtins) {
  CelTypeRegistry registry;

  // simple
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> bool_type,
                       registry.GetTypeProvider().FindType("bool"));
  EXPECT_THAT(bool_type, Optional(TypeNameIs("bool")));
  // opaque
  ASSERT_OK_AND_ASSIGN(
      absl::optional<Type> timestamp_type,
      registry.GetTypeProvider().FindType("google.protobuf.Timestamp"));
  EXPECT_THAT(timestamp_type,
              Optional(TypeNameIs("google.protobuf.Timestamp")));
  // wrapper
  ASSERT_OK_AND_ASSIGN(
      absl::optional<Type> int_wrapper_type,
      registry.GetTypeProvider().FindType("google.protobuf.Int64Value"));
  EXPECT_THAT(int_wrapper_type,
              Optional(TypeNameIs("google.protobuf.Int64Value")));
  // json
  ASSERT_OK_AND_ASSIGN(
      absl::optional<Type> json_struct_type,
      registry.GetTypeProvider().FindType("google.protobuf.Struct"));
  EXPECT_THAT(json_struct_type, Optional(TypeNameIs("map")));
  // special
  ASSERT_OK_AND_ASSIGN(
      absl::optional<Type> any_type,
      registry.GetTypeProvider().FindType("google.protobuf.Any"));
  EXPECT_THAT(any_type, Optional(TypeNameIs("google.protobuf.Any")));
}

}  // namespace

}  // namespace google::api::expr::runtime
