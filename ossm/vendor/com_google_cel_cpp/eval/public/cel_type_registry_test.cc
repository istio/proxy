#include "eval/public/cel_type_registry.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/type_provider.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/type_manager.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/legacy_value_manager.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::MemoryManagerRef;
using ::cel::Type;
using ::cel::TypeFactory;
using ::cel::TypeManager;
using ::cel::TypeProvider;
using ::cel::ValueManager;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Key;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

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
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto type_provider = registry.GetFirstTypeProvider();
  ASSERT_NE(type_provider, nullptr);
  ASSERT_TRUE(
      type_provider->ProvideLegacyType("google.protobuf.Int64").has_value());
  ASSERT_FALSE(
      type_provider->ProvideLegacyType("google.protobuf.Any").has_value());
}

TEST(CelTypeRegistryTest, TestGetFirstTypeProviderFailureOnEmpty) {
  CelTypeRegistry registry;
  auto type_provider = registry.GetFirstTypeProvider();
  ASSERT_EQ(type_provider, nullptr);
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFound) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
  auto desc = registry.FindTypeAdapter("google.protobuf.Any");
  ASSERT_TRUE(desc.has_value());
}

TEST(CelTypeRegistryTest, TestFindTypeAdapterFoundMultipleProviders) {
  CelTypeRegistry registry;
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Int64"}));
  registry.RegisterTypeProvider(std::make_unique<TestTypeProvider>(
      std::vector<std::string>{"google.protobuf.Any"}));
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

  cel::common_internal::LegacyValueManager value_factory(
      MemoryManagerRef::ReferenceCounting(), registry.GetTypeProvider());

  // simple
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> bool_type,
                       value_factory.FindType("bool"));
  EXPECT_THAT(bool_type, Optional(TypeNameIs("bool")));
  // opaque
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> timestamp_type,
                       value_factory.FindType("google.protobuf.Timestamp"));
  EXPECT_THAT(timestamp_type,
              Optional(TypeNameIs("google.protobuf.Timestamp")));
  // wrapper
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> int_wrapper_type,
                       value_factory.FindType("google.protobuf.Int64Value"));
  EXPECT_THAT(int_wrapper_type,
              Optional(TypeNameIs("google.protobuf.Int64Value")));
  // json
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> json_struct_type,
                       value_factory.FindType("google.protobuf.Struct"));
  EXPECT_THAT(json_struct_type, Optional(TypeNameIs("map")));
  // special
  ASSERT_OK_AND_ASSIGN(absl::optional<Type> any_type,
                       value_factory.FindType("google.protobuf.Any"));
  EXPECT_THAT(any_type, Optional(TypeNameIs("google.protobuf.Any")));
}

}  // namespace

}  // namespace google::api::expr::runtime
