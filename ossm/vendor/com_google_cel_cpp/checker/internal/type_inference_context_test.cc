// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/type_inference_context.h"

#include <utility>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SafeMatcherCast;
using ::testing::SizeIs;

MATCHER_P(IsTypeParam, param, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kTypeParam) {
    return false;
  }
  TypeParamType type = got.GetTypeParam();

  return type.name() == param;
}

MATCHER_P(IsListType, elems_matcher, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kList) {
    return false;
  }
  ListType type = got.GetList();

  Type elem = type.element();
  return SafeMatcherCast<Type>(elems_matcher)
      .MatchAndExplain(elem, result_listener);
}

MATCHER_P2(IsMapType, key_matcher, value_matcher, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kMap) {
    return false;
  }
  MapType type = got.GetMap();

  Type key = type.key();
  Type value = type.value();
  return SafeMatcherCast<Type>(key_matcher)
             .MatchAndExplain(key, result_listener) &&
         SafeMatcherCast<Type>(value_matcher)
             .MatchAndExplain(value, result_listener);
}

MATCHER_P(IsTypeKind, kind, "") {
  const Type& got = arg;
  TypeKind want_kind = kind;
  if (got.kind() == want_kind) {
    return true;
  }
  *result_listener << "got: " << TypeKindToString(got.kind());
  *result_listener << "\n";
  *result_listener << "wanted: " << TypeKindToString(want_kind);
  return false;
}

MATCHER_P(IsTypeType, matcher, "") {
  const Type& got = arg;

  if (got.kind() != TypeKind::kType) {
    return false;
  }

  TypeType type_type = got.GetType();
  if (type_type.GetParameters().size() != 1) {
    return false;
  }

  return SafeMatcherCast<Type>(matcher).MatchAndExplain(got.GetParameters()[0],
                                                        result_listener);
}

TEST(TypeInferenceContextTest, InstantiateTypeParams) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type = context.InstantiateTypeParams(TypeParamType("MyType"));
  EXPECT_THAT(type, IsTypeParam("T%1"));
  Type type2 = context.InstantiateTypeParams(TypeParamType("MyType"));
  EXPECT_THAT(type2, IsTypeParam("T%2"));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsWithSubstitutions) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  TypeInferenceContext::InstanceMap instance_map;
  Type type =
      context.InstantiateTypeParams(TypeParamType("MyType"), instance_map);
  EXPECT_THAT(type, IsTypeParam("T%1"));
  Type type2 =
      context.InstantiateTypeParams(TypeParamType("MyType"), instance_map);
  EXPECT_THAT(type2, IsTypeParam("T%1"));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsUnparameterized) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type type = context.InstantiateTypeParams(IntType());
  EXPECT_TRUE(type.IsInt());
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsList) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type list_type = ListType(&arena, TypeParamType("MyType"));

  Type type = context.InstantiateTypeParams(list_type);
  EXPECT_THAT(type, IsListType(IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsListPrimitive) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type list_type = ListType(&arena, IntType());

  Type type = context.InstantiateTypeParams(list_type);
  EXPECT_THAT(type, IsListType(IsTypeKind(TypeKind::kInt)));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMap) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, TypeParamType("K"), TypeParamType("V"));

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeParam("T%1"), IsTypeParam("T%2")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMapSameParam) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, TypeParamType("E"), TypeParamType("E"));

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeParam("T%1"), IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMapPrimitive) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, StringType(), IntType());

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeKind(TypeKind::kString),
                              IsTypeKind(TypeKind::kInt)));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsType) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type_type = TypeType(&arena, TypeParamType("T"));

  Type type = context.InstantiateTypeParams(type_type);
  EXPECT_THAT(type, IsTypeType(IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsTypeEmpty) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type_type = TypeType();

  Type type = context.InstantiateTypeParams(type_type);
  EXPECT_THAT(type, IsTypeKind(TypeKind::kType));
  EXPECT_THAT(type.AsType()->GetParameters(), IsEmpty());
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsOpaque) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  std::vector<Type> parameters = {TypeParamType("T"), IntType(),
                                  TypeParamType("U"), TypeParamType("T")};

  Type type_type = OpaqueType(&arena, "MyTuple", parameters);

  Type type = context.InstantiateTypeParams(type_type);
  ASSERT_THAT(type, IsTypeKind(TypeKind::kOpaque));
  EXPECT_EQ(type.AsOpaque()->name(), "MyTuple");
  EXPECT_THAT(type.AsOpaque()->GetParameters(),
              ElementsAre(IsTypeParam("T%1"), IsTypeKind(TypeKind::kInt),
                          IsTypeParam("T%2"), IsTypeParam("T%1")));
}

// TODO(uncreated-issue/72): Does not consider any substitutions based on type
// inferences yet.
TEST(TypeInferenceContextTest, OpaqueTypeAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  std::vector<Type> parameters = {TypeParamType("T"), IntType()};

  Type type_type = OpaqueType(&arena, "MyTuple", parameters);

  Type type = context.InstantiateTypeParams(type_type);
  ASSERT_THAT(type, IsTypeKind(TypeKind::kOpaque));
  EXPECT_TRUE(context.IsAssignable(type, type));
}

TEST(TypeInferenceContextTest, WrapperTypeAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  EXPECT_TRUE(context.IsAssignable(StringType(), StringWrapperType()));
  EXPECT_TRUE(context.IsAssignable(NullType(), StringWrapperType()));
}

TEST(TypeInferenceContextTest, MismatchedTypeNotAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  EXPECT_FALSE(context.IsAssignable(IntType(), StringWrapperType()));
}

TEST(TypeInferenceContextTest, OverloadResolution) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      auto decl,
      MakeFunctionDecl(
          "foo",
          MakeOverloadDecl("foo_int_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("foo_double_double", DoubleType(), DoubleType(),
                           DoubleType())));

  auto resolution = context.ResolveOverload(decl, {IntType(), IntType()},
                                            /*is_receiver=*/false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_THAT(resolution->result_type, IsTypeKind(TypeKind::kInt));
  EXPECT_THAT(resolution->overloads, SizeIs(1));
}

TEST(TypeInferenceContextTest, MultipleOverloadsResultTypeDyn) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      auto decl,
      MakeFunctionDecl(
          "foo",
          MakeOverloadDecl("foo_int_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("foo_double_double", DoubleType(), DoubleType(),
                           DoubleType())));

  auto resolution = context.ResolveOverload(decl, {DynType(), DynType()},
                                            /*is_receiver=*/false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_THAT(resolution->result_type, IsTypeKind(TypeKind::kDyn));
  EXPECT_THAT(resolution->overloads, SizeIs(2));
}

MATCHER_P(IsOverloadDecl, name, "") {
  const OverloadDecl& got = arg;
  return got.id() == name;
}

TEST(TypeInferenceContextTest, ResolveOverloadBasic) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_+_", MakeOverloadDecl("add_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("add_double", DoubleType(), DoubleType(),
                           DoubleType())));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {IntType(), IntType()}, false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_THAT(resolution->result_type, IsTypeKind(TypeKind::kInt));
  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("add_int")));
}

TEST(TypeInferenceContextTest, ResolveOverloadFails) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_+_", MakeOverloadDecl("add_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("add_double", DoubleType(), DoubleType(),
                           DoubleType())));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {IntType(), DoubleType()}, false);
  ASSERT_FALSE(resolution.has_value());
}

TEST(TypeInferenceContextTest, ResolveOverloadWithParamsNoMatch) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_==_", MakeOverloadDecl("equals", BoolType(), TypeParamType("A"),
                                   TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {IntType(), DoubleType()}, false);
  ASSERT_FALSE(resolution.has_value());
}

TEST(TypeInferenceContextTest, ResolveOverloadWithMixedParamsMatch) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_==_", MakeOverloadDecl("equals", BoolType(), TypeParamType("A"),
                                   TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {list_of_a, list_of_a}, false);
  ASSERT_TRUE(resolution.has_value()) << context.DebugString();
}

TEST(TypeInferenceContextTest, ResolveOverloadWithMixedParamsMatch2) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));
  Type list_of_int = ListType(&arena, IntType());

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_==_", MakeOverloadDecl("equals", BoolType(), TypeParamType("A"),
                                   TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {list_of_a, list_of_int}, false);
  ASSERT_TRUE(resolution.has_value()) << context.DebugString();
  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("equals")));
}

TEST(TypeInferenceContextTest, ResolveOverloadWithParamsMatches) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_==_", MakeOverloadDecl("equals", BoolType(), TypeParamType("A"),
                                   TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {IntType(), IntType()}, false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_TRUE(resolution->result_type.IsBool());
  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("equals")));
}

TEST(TypeInferenceContextTest, ResolveOverloadWithNestedParamsMatch) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));
  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl("_+_", MakeOverloadDecl("add_list", list_of_a, list_of_a,
                                               list_of_a)));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(
          decl, {list_of_a_instance, ListType(&arena, IntType())}, false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_TRUE(resolution->result_type.IsList());

  EXPECT_THAT(
      context.FinalizeType(resolution->result_type).AsList()->GetElement(),
      IsTypeKind(TypeKind::kInt))
      << context.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("add_list")));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution2 =
      context.ResolveOverload(
          decl, {ListType(&arena, IntType()), list_of_a_instance}, false);
  ASSERT_TRUE(resolution2.has_value());
  EXPECT_TRUE(resolution2->result_type.IsList());

  EXPECT_THAT(
      context.FinalizeType(resolution2->result_type).AsList()->GetElement(),
      IsTypeKind(TypeKind::kInt))
      << context.DebugString();

  EXPECT_THAT(resolution2->overloads, ElementsAre(IsOverloadDecl("add_list")));
}

TEST(TypeInferenceContextTest, ResolveOverloadWithNestedParamsNoMatch) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));
  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl("_+_", MakeOverloadDecl("add_list", list_of_a, list_of_a,
                                               list_of_a)));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {list_of_a_instance, IntType()}, false);
  EXPECT_FALSE(resolution.has_value());
}

TEST(TypeInferenceContextTest, InferencesAccumulate) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));
  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl("_+_", MakeOverloadDecl("add_list", list_of_a, list_of_a,
                                               list_of_a)));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  absl::optional<TypeInferenceContext::OverloadResolution> resolution1 =
      context.ResolveOverload(decl, {list_of_a_instance, list_of_a_instance},
                              false);
  ASSERT_TRUE(resolution1.has_value());
  EXPECT_TRUE(resolution1->result_type.IsList());

  absl::optional<TypeInferenceContext::OverloadResolution> resolution2 =
      context.ResolveOverload(
          decl, {resolution1->result_type, ListType(&arena, IntType())}, false);
  ASSERT_TRUE(resolution2.has_value());
  EXPECT_TRUE(resolution2->result_type.IsList());

  EXPECT_THAT(
      context.FinalizeType(resolution2->result_type).AsList()->GetElement(),
      IsTypeKind(TypeKind::kInt));

  EXPECT_THAT(resolution2->overloads, ElementsAre(IsOverloadDecl("add_list")));
}

TEST(TypeInferenceContextTest, DebugString) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));
  Type list_of_int = ListType(&arena, IntType());
  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl("_+_", MakeOverloadDecl("add_list", list_of_a, list_of_a,
                                               list_of_a)));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {list_of_int, list_of_int}, false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_TRUE(resolution->result_type.IsList());

  EXPECT_EQ(context.DebugString(), "type_parameter_bindings: T%1 (A) -> int");
}

struct TypeInferenceContextWrapperTypesTestCase {
  Type wrapper_type;
  Type wrapped_primitive_type;
};

class TypeInferenceContextWrapperTypesTest
    : public ::testing::TestWithParam<
          TypeInferenceContextWrapperTypesTestCase> {
 public:
  TypeInferenceContextWrapperTypesTest() : context_(&arena_) {
    auto decl = MakeFunctionDecl(
        "_?_:_",
        MakeOverloadDecl("ternary",
                         /*result_type=*/TypeParamType("A"), BoolType(),
                         TypeParamType("A"), TypeParamType("A")));

    ABSL_CHECK_OK(decl.status());
    ternary_decl_ = *std::move(decl);
  }

 protected:
  google::protobuf::Arena arena_;
  TypeInferenceContext context_{&arena_};
  FunctionDecl ternary_decl_;
};

TEST_P(TypeInferenceContextWrapperTypesTest, ResolvePrimitiveArg) {
  const TypeInferenceContextWrapperTypesTestCase& test_case = GetParam();

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context_.ResolveOverload(ternary_decl_,
                               {BoolType(), test_case.wrapper_type,
                                test_case.wrapped_primitive_type},
                               false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context_.FinalizeType(resolution->result_type),
              IsTypeKind(test_case.wrapper_type.kind()))
      << context_.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

TEST_P(TypeInferenceContextWrapperTypesTest, ResolveWrapperArg) {
  const TypeInferenceContextWrapperTypesTestCase& test_case = GetParam();

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context_.ResolveOverload(
          ternary_decl_,
          {BoolType(), test_case.wrapper_type, test_case.wrapper_type}, false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context_.FinalizeType(resolution->result_type),
              IsTypeKind(test_case.wrapper_type.kind()))
      << context_.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

TEST_P(TypeInferenceContextWrapperTypesTest, ResolveNullArg) {
  const TypeInferenceContextWrapperTypesTestCase& test_case = GetParam();

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context_.ResolveOverload(ternary_decl_,
                               {BoolType(), test_case.wrapper_type, NullType()},
                               false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context_.FinalizeType(resolution->result_type),
              IsTypeKind(test_case.wrapper_type.kind()))
      << context_.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

TEST_P(TypeInferenceContextWrapperTypesTest, NullWidens) {
  const TypeInferenceContextWrapperTypesTestCase& test_case = GetParam();

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context_.ResolveOverload(ternary_decl_,
                               {BoolType(), NullType(), test_case.wrapper_type},
                               false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context_.FinalizeType(resolution->result_type),
              IsTypeKind(test_case.wrapper_type.kind()))
      << context_.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

TEST_P(TypeInferenceContextWrapperTypesTest, PrimitiveWidens) {
  const TypeInferenceContextWrapperTypesTestCase& test_case = GetParam();

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context_.ResolveOverload(ternary_decl_,
                               {BoolType(), test_case.wrapped_primitive_type,
                                test_case.wrapper_type},
                               false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context_.FinalizeType(resolution->result_type),
              IsTypeKind(test_case.wrapper_type.kind()))
      << context_.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

INSTANTIATE_TEST_SUITE_P(
    Types, TypeInferenceContextWrapperTypesTest,
    ::testing::Values(
        TypeInferenceContextWrapperTypesTestCase{IntWrapperType(), IntType()},
        TypeInferenceContextWrapperTypesTestCase{UintWrapperType(), UintType()},
        TypeInferenceContextWrapperTypesTestCase{DoubleWrapperType(),
                                                 DoubleType()},
        TypeInferenceContextWrapperTypesTestCase{StringWrapperType(),
                                                 StringType()},
        TypeInferenceContextWrapperTypesTestCase{BytesWrapperType(),
                                                 BytesType()},
        TypeInferenceContextWrapperTypesTestCase{BoolWrapperType(), BoolType()},
        TypeInferenceContextWrapperTypesTestCase{DynType(), IntType()}));

TEST(TypeInferenceContextTest, ResolveOverloadWithUnionTypePromotion) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl(
          "_?_:_",
          MakeOverloadDecl("ternary",
                           /*result_type=*/TypeParamType("A"), BoolType(),
                           TypeParamType("A"), TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {BoolType(), NullType(), IntWrapperType()},
                              false);
  ASSERT_TRUE(resolution.has_value());

  EXPECT_THAT(context.FinalizeType(resolution->result_type),
              IsTypeKind(TypeKind::kIntWrapper))
      << context.DebugString();

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("ternary")));
}

// TypeType has special handling (differently-parameterized type-types are
// always assignable for the sake of comparisons).
TEST(TypeInferenceContextTest, ResolveOverloadWithTypeType) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl decl,
      MakeFunctionDecl("type",
                       MakeOverloadDecl("to_type",
                                        /*result_type=*/
                                        TypeType(&arena, TypeParamType("A")),
                                        TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(decl, {StringType()}, false);
  ASSERT_TRUE(resolution.has_value());

  auto result_type = context.FinalizeType(resolution->result_type);
  ASSERT_THAT(result_type, IsTypeKind(TypeKind::kType));

  EXPECT_THAT(result_type.AsType()->GetParameters(),
              ElementsAre(IsTypeKind(TypeKind::kString)));

  EXPECT_THAT(resolution->overloads, ElementsAre(IsOverloadDecl("to_type")));
}

TEST(TypeInferenceContextTest, ResolveOverloadWithInferredTypeType) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl to_type_decl,
      MakeFunctionDecl("type",
                       MakeOverloadDecl("to_type",
                                        /*result_type=*/
                                        TypeType(&arena, TypeParamType("A")),
                                        TypeParamType("A"))));

  ASSERT_OK_AND_ASSIGN(
      FunctionDecl equals_decl,
      MakeFunctionDecl("_==_", MakeOverloadDecl("equals",
                                                /*result_type=*/
                                                BoolType(), TypeParamType("A"),
                                                TypeParamType("A"))));

  absl::optional<TypeInferenceContext::OverloadResolution> resolution =
      context.ResolveOverload(to_type_decl, {StringType()}, false);
  ASSERT_TRUE(resolution.has_value());

  auto lhs_result_type = resolution->result_type;
  ASSERT_THAT(lhs_result_type, IsTypeKind(TypeKind::kType));

  resolution = context.ResolveOverload(to_type_decl, {IntType()}, false);
  ASSERT_TRUE(resolution.has_value());

  auto rhs_result_type = resolution->result_type;
  ASSERT_THAT(rhs_result_type, IsTypeKind(TypeKind::kType));

  resolution = context.ResolveOverload(
      equals_decl, {rhs_result_type, lhs_result_type}, false);
  ASSERT_TRUE(resolution.has_value());
  auto result_type = context.FinalizeType(resolution->result_type);
  ASSERT_THAT(result_type, IsTypeKind(TypeKind::kBool));

  auto inferred_lhs = context.FinalizeType(lhs_result_type);
  auto inferred_rhs = context.FinalizeType(rhs_result_type);

  ASSERT_THAT(inferred_rhs, IsTypeKind(TypeKind::kType));
  ASSERT_THAT(inferred_lhs, IsTypeKind(TypeKind::kType));

  ASSERT_THAT(inferred_lhs.AsType()->GetParameters(),
              ElementsAre(IsTypeKind(TypeKind::kString)));
  ASSERT_THAT(inferred_rhs.AsType()->GetParameters(),
              ElementsAre(IsTypeKind(TypeKind::kInt)));
}

TEST(TypeInferenceContextTest, AssignabilityContext) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  {
    auto assignability_context = context.CreateAssignabilityContext();
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntType(), list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntType(), list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntWrapperType(), list_of_a_instance.AsList()->GetElement()));

    assignability_context.UpdateInferredTypeAssignments();
  }
  Type resolved_type = context.FinalizeType(list_of_a_instance);

  ASSERT_THAT(resolved_type, IsTypeKind(TypeKind::kList));
  EXPECT_THAT(resolved_type.AsList()->GetElement(),
              IsTypeKind(TypeKind::kIntWrapper));
}

TEST(TypeInferenceContextTest, AssignabilityContextAbstractType) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  {
    auto assignability_context = context.CreateAssignabilityContext();
    EXPECT_TRUE(assignability_context.IsAssignable(
        OptionalType(&arena, IntType()),
        list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        OptionalType(&arena, DynType()),
        list_of_a_instance.AsList()->GetElement()));

    assignability_context.UpdateInferredTypeAssignments();
  }
  Type resolved_type = context.FinalizeType(list_of_a_instance);

  ASSERT_THAT(resolved_type, IsTypeKind(TypeKind::kList));
  ASSERT_THAT(resolved_type.AsList()->GetElement(),
              IsTypeKind(TypeKind::kOpaque));
  EXPECT_THAT(resolved_type.AsList()->GetElement().AsOpaque()->name(),
              "optional_type");
  EXPECT_THAT(resolved_type.AsList()->GetElement().AsOpaque()->GetParameters(),
              ElementsAre(IsTypeKind(TypeKind::kDyn)));
}

TEST(TypeInferenceContextTest, AssignabilityContextAbstractTypeWrapper) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  {
    auto assignability_context = context.CreateAssignabilityContext();
    EXPECT_TRUE(assignability_context.IsAssignable(
        OptionalType(&arena, IntType()),
        list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        OptionalType(&arena, IntWrapperType()),
        list_of_a_instance.AsList()->GetElement()));

    assignability_context.UpdateInferredTypeAssignments();
  }
  Type resolved_type = context.FinalizeType(list_of_a_instance);

  ASSERT_THAT(resolved_type, IsTypeKind(TypeKind::kList));
  ASSERT_THAT(resolved_type.AsList()->GetElement(),
              IsTypeKind(TypeKind::kOpaque));
  EXPECT_THAT(resolved_type.AsList()->GetElement().AsOpaque()->name(),
              "optional_type");
  EXPECT_THAT(resolved_type.AsList()->GetElement().AsOpaque()->GetParameters(),
              ElementsAre(IsTypeKind(TypeKind::kIntWrapper)));
}

TEST(TypeInferenceContextTest, AssignabilityContextNotApplied) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  {
    auto assignability_context = context.CreateAssignabilityContext();
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntType(), list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntType(), list_of_a_instance.AsList()->GetElement()));
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntWrapperType(), list_of_a_instance.AsList()->GetElement()));
  }

  Type resolved_type = context.FinalizeType(list_of_a_instance);

  ASSERT_THAT(resolved_type, IsTypeKind(TypeKind::kList));
  EXPECT_THAT(resolved_type.AsList()->GetElement(), IsTypeKind(TypeKind::kDyn));
}

TEST(TypeInferenceContextTest, AssignabilityContextReset) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type list_of_a = ListType(&arena, TypeParamType("A"));

  Type list_of_a_instance = context.InstantiateTypeParams(list_of_a);

  {
    auto assignability_context = context.CreateAssignabilityContext();
    EXPECT_TRUE(assignability_context.IsAssignable(
        IntType(), list_of_a_instance.AsList()->GetElement()));
    assignability_context.Reset();
    EXPECT_TRUE(assignability_context.IsAssignable(
        DoubleType(), list_of_a_instance.AsList()->GetElement()));
    assignability_context.UpdateInferredTypeAssignments();
  }

  Type resolved_type = context.FinalizeType(list_of_a_instance);

  ASSERT_THAT(resolved_type, IsTypeKind(TypeKind::kList));
  EXPECT_THAT(resolved_type.AsList()->GetElement(),
              IsTypeKind(TypeKind::kDouble));
}

}  // namespace
}  // namespace cel::checker_internal
