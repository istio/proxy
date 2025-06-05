// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/portable_cel_expr_builder_factory.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/container/node_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/proto_time_encoding.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::protobuf::Int64Value;

// Helpers for c++ / proto to cel value conversions.
absl::optional<CelValue> Unwrap(const google::protobuf::MessageLite* wrapper) {
  if (wrapper->GetTypeName() == "google.protobuf.Duration") {
    const auto* duration =
        cel::internal::down_cast<const google::protobuf::Duration*>(wrapper);
    return CelValue::CreateDuration(cel::internal::DecodeDuration(*duration));
  } else if (wrapper->GetTypeName() == "google.protobuf.Timestamp") {
    const auto* timestamp =
        cel::internal::down_cast<const google::protobuf::Timestamp*>(wrapper);
    return CelValue::CreateTimestamp(cel::internal::DecodeTime(*timestamp));
  }
  return absl::nullopt;
}

struct NativeToCelValue {
  template <typename T>
  absl::optional<CelValue> Convert(T arg) const {
    return absl::nullopt;
  }

  absl::optional<CelValue> Convert(int64_t v) const {
    return CelValue::CreateInt64(v);
  }

  absl::optional<CelValue> Convert(const std::string& str) const {
    return CelValue::CreateString(&str);
  }

  absl::optional<CelValue> Convert(double v) const {
    return CelValue::CreateDouble(v);
  }

  absl::optional<CelValue> Convert(bool v) const {
    return CelValue::CreateBool(v);
  }

  absl::optional<CelValue> Convert(const Int64Value& v) const {
    return CelValue::CreateInt64(v.value());
  }
};

template <typename MessageT, typename FieldT>
class FieldImpl;

template <typename MessageT>
class ProtoField {
 public:
  template <typename FieldT>
  using FieldImpl = FieldImpl<MessageT, FieldT>;

  virtual ~ProtoField() = default;
  virtual absl::Status Set(MessageT* m, CelValue v) const = 0;
  virtual absl::StatusOr<CelValue> Get(const MessageT* m) const = 0;
  virtual bool Has(const MessageT* m) const = 0;
};

// template helpers for wrapping member accessors generically.
template <typename MessageT, typename FieldT>
struct ScalarApiWrap {
  using GetFn = FieldT (MessageT::*)() const;
  using HasFn = bool (MessageT::*)() const;
  using SetFn = void (MessageT::*)(FieldT);

  ScalarApiWrap(GetFn get_fn, HasFn has_fn, SetFn set_fn)
      : get_fn(get_fn), has_fn(has_fn), set_fn(set_fn) {}

  FieldT InvokeGet(const MessageT* msg) const {
    return std::invoke(get_fn, msg);
  }
  bool InvokeHas(const MessageT* msg) const {
    if (has_fn == nullptr) return true;
    return std::invoke(has_fn, msg);
  }
  void InvokeSet(MessageT* msg, FieldT arg) const {
    if (set_fn != nullptr) {
      std::invoke(set_fn, msg, arg);
    }
  }

  GetFn get_fn;
  HasFn has_fn;
  SetFn set_fn;
};

template <typename MessageT, typename FieldT>
struct ComplexTypeApiWrap {
 public:
  using GetFn = const FieldT& (MessageT::*)() const;
  using HasFn = bool (MessageT::*)() const;
  using SetAllocatedFn = void (MessageT::*)(FieldT*);

  ComplexTypeApiWrap(GetFn get_fn, HasFn has_fn,
                     SetAllocatedFn set_allocated_fn)
      : get_fn(get_fn), has_fn(has_fn), set_allocated_fn(set_allocated_fn) {}

  const FieldT& InvokeGet(const MessageT* msg) const {
    return std::invoke(get_fn, msg);
  }
  bool InvokeHas(const MessageT* msg) const {
    if (has_fn == nullptr) return true;
    return std::invoke(has_fn, msg);
  }

  void InvokeSetAllocated(MessageT* msg, FieldT* arg) const {
    if (set_allocated_fn != nullptr) {
      std::invoke(set_allocated_fn, msg, arg);
    }
  }

  GetFn get_fn;
  HasFn has_fn;
  SetAllocatedFn set_allocated_fn;
};

template <typename MessageT, typename FieldT>
class FieldImpl : public ProtoField<MessageT> {
 private:
  using ApiWrap = ScalarApiWrap<MessageT, FieldT>;

 public:
  FieldImpl(typename ApiWrap::GetFn get_fn, typename ApiWrap::HasFn has_fn,
            typename ApiWrap::SetFn set_fn)
      : api_wrapper_(get_fn, has_fn, set_fn) {}
  absl::Status Set(TestMessage* m, CelValue v) const override {
    FieldT arg;
    if (!v.GetValue(&arg)) {
      return absl::InvalidArgumentError("wrong type for set");
    }
    api_wrapper_.InvokeSet(m, arg);
    return absl::OkStatus();
  }

  absl::StatusOr<CelValue> Get(const TestMessage* m) const override {
    FieldT result = api_wrapper_.InvokeGet(m);
    auto converted = NativeToCelValue().Convert(result);
    if (converted.has_value()) {
      return *converted;
    }
    return absl::UnimplementedError("not implemented for type");
  }

  bool Has(const TestMessage* m) const override {
    return api_wrapper_.InvokeHas(m);
  }

 private:
  ApiWrap api_wrapper_;
};

template <typename MessageT>
class FieldImpl<MessageT, Int64Value> : public ProtoField<MessageT> {
  using ApiWrap = ComplexTypeApiWrap<MessageT, Int64Value>;

 public:
  FieldImpl(typename ApiWrap::GetFn get_fn, typename ApiWrap::HasFn has_fn,
            typename ApiWrap::SetAllocatedFn set_fn)
      : api_wrapper_(get_fn, has_fn, set_fn) {}
  absl::Status Set(TestMessage* m, CelValue v) const override {
    int64_t arg;
    if (!v.GetValue(&arg)) {
      return absl::InvalidArgumentError("wrong type for set");
    }
    Int64Value* proto_value = new Int64Value();
    proto_value->set_value(arg);
    api_wrapper_.InvokeSetAllocated(m, proto_value);
    return absl::OkStatus();
  }

  absl::StatusOr<CelValue> Get(const TestMessage* m) const override {
    if (!api_wrapper_.InvokeHas(m)) {
      return CelValue::CreateNull();
    }
    Int64Value result = api_wrapper_.InvokeGet(m);
    auto converted = NativeToCelValue().Convert(std::move(result));
    if (converted.has_value()) {
      return *converted;
    }
    return absl::UnimplementedError("not implemented for type");
  }

  bool Has(const TestMessage* m) const override {
    return api_wrapper_.InvokeHas(m);
  }

 private:
  ApiWrap api_wrapper_;
};

// Simple type system for Testing.
class DemoTypeProvider;

class DemoTimestamp : public LegacyTypeInfoApis, public LegacyTypeMutationApis {
 public:
  DemoTimestamp() {}

  std::string DebugString(
      const MessageWrapper& wrapped_message) const override {
    return std::string(GetTypename(wrapped_message));
  }

  absl::string_view GetTypename(
      const MessageWrapper& wrapped_message) const override {
    return "google.protobuf.Timestamp";
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override {
    return nullptr;
  }

  bool DefinesField(absl::string_view field_name) const override {
    return field_name == "seconds" || field_name == "nanos";
  }

  absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManagerRef memory_manager) const override;

  absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder instance) const override;

  absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder& instance) const override;

 private:
  absl::Status Validate(const google::protobuf::MessageLite* wrapped_message) const {
    if (wrapped_message->GetTypeName() != "google.protobuf.Timestamp") {
      return absl::InvalidArgumentError("not a timestamp");
    }
    return absl::OkStatus();
  }
};

class DemoTypeInfo : public LegacyTypeInfoApis {
 public:
  explicit DemoTypeInfo(const DemoTypeProvider* owning_provider)
      : owning_provider_(*owning_provider) {}
  std::string DebugString(const MessageWrapper& wrapped_message) const override;

  absl::string_view GetTypename(
      const MessageWrapper& wrapped_message) const override;

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override;

 private:
  const DemoTypeProvider& owning_provider_;
};

class DemoTestMessage : public LegacyTypeInfoApis,
                        public LegacyTypeMutationApis,
                        public LegacyTypeAccessApis {
 public:
  explicit DemoTestMessage(const DemoTypeProvider* owning_provider);

  std::string DebugString(
      const MessageWrapper& wrapped_message) const override {
    return std::string(GetTypename(wrapped_message));
  }

  absl::string_view GetTypename(
      const MessageWrapper& wrapped_message) const override {
    return "google.api.expr.runtime.TestMessage";
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  const LegacyTypeMutationApis* GetMutationApis(
      const MessageWrapper& wrapped_message) const override {
    return this;
  }

  absl::optional<FieldDescription> FindFieldByName(
      absl::string_view name) const override {
    if (auto it = fields_.find(name); it != fields_.end()) {
      return FieldDescription{0, name};
    }
    return absl::nullopt;
  }

  bool DefinesField(absl::string_view field_name) const override {
    return fields_.contains(field_name);
  }

  absl::StatusOr<CelValue::MessageWrapper::Builder> NewInstance(
      cel::MemoryManagerRef memory_manager) const override;

  absl::StatusOr<CelValue> AdaptFromWellKnownType(
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder instance) const override;

  absl::Status SetField(
      absl::string_view field_name, const CelValue& value,
      cel::MemoryManagerRef memory_manager,
      CelValue::MessageWrapper::Builder& instance) const override;

  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override;

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManagerRef memory_manager) const override;

  std::vector<absl::string_view> ListFields(
      const CelValue::MessageWrapper& instance) const override {
    std::vector<absl::string_view> fields;
    fields.reserve(fields_.size());
    for (const auto& field : fields_) {
      fields.emplace_back(field.first);
    }
    return fields;
  }

 private:
  using Field = ProtoField<TestMessage>;
  const DemoTypeProvider& owning_provider_;
  absl::flat_hash_map<absl::string_view, std::unique_ptr<Field>> fields_;
};

class DemoTypeProvider : public LegacyTypeProvider {
 public:
  DemoTypeProvider() : timestamp_type_(), test_message_(this), info_(this) {}
  const LegacyTypeInfoApis* GetTypeInfoInstance() const { return &info_; }

  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const override {
    if (name == "google.protobuf.Timestamp") {
      return LegacyTypeAdapter(nullptr, &timestamp_type_);
    } else if (name == "google.api.expr.runtime.TestMessage") {
      return LegacyTypeAdapter(&test_message_, &test_message_);
    }
    return absl::nullopt;
  }

  absl::optional<const LegacyTypeInfoApis*> ProvideLegacyTypeInfo(
      absl::string_view name) const override {
    if (name == "google.protobuf.Timestamp") {
      return &timestamp_type_;
    } else if (name == "google.api.expr.runtime.TestMessage") {
      return &test_message_;
    }
    return absl::nullopt;
  }

  const std::string& GetStableType(
      const google::protobuf::MessageLite* wrapped_message) const {
    std::string name(wrapped_message->GetTypeName());
    auto [iter, inserted] = stable_types_.insert(name);
    return *iter;
  }

  CelValue WrapValue(const google::protobuf::MessageLite* message) const {
    return CelValue::CreateMessageWrapper(
        CelValue::MessageWrapper(message, GetTypeInfoInstance()));
  }

 private:
  DemoTimestamp timestamp_type_;
  DemoTestMessage test_message_;
  DemoTypeInfo info_;
  mutable absl::node_hash_set<std::string> stable_types_;  // thread hostile
};

std::string DemoTypeInfo::DebugString(
    const MessageWrapper& wrapped_message) const {
  return std::string(wrapped_message.message_ptr()->GetTypeName());
}

absl::string_view DemoTypeInfo::GetTypename(
    const MessageWrapper& wrapped_message) const {
  return owning_provider_.GetStableType(wrapped_message.message_ptr());
}

const LegacyTypeAccessApis* DemoTypeInfo::GetAccessApis(
    const MessageWrapper& wrapped_message) const {
  auto adapter = owning_provider_.ProvideLegacyType(
      wrapped_message.message_ptr()->GetTypeName());
  if (adapter.has_value()) {
    return adapter->access_apis();
  }
  return nullptr;  // not implemented yet.
}

absl::StatusOr<CelValue::MessageWrapper::Builder> DemoTimestamp::NewInstance(
    cel::MemoryManagerRef memory_manager) const {
  auto* ts = google::protobuf::Arena::Create<google::protobuf::Timestamp>(
      cel::extensions::ProtoMemoryManagerArena(memory_manager));
  return CelValue::MessageWrapper::Builder(ts);
}
absl::StatusOr<CelValue> DemoTimestamp::AdaptFromWellKnownType(
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder instance) const {
  auto value = Unwrap(instance.message_ptr());
  ABSL_ASSERT(value.has_value());
  return *value;
}

absl::Status DemoTimestamp::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  ABSL_ASSERT(Validate(instance.message_ptr()).ok());
  auto* mutable_ts = cel::internal::down_cast<google::protobuf::Timestamp*>(
      instance.message_ptr());
  if (field_name == "seconds" && value.IsInt64()) {
    mutable_ts->set_seconds(value.Int64OrDie());
  } else if (field_name == "nanos" && value.IsInt64()) {
    mutable_ts->set_nanos(value.Int64OrDie());
  } else {
    return absl::UnknownError("no such field");
  }
  return absl::OkStatus();
}

DemoTestMessage::DemoTestMessage(const DemoTypeProvider* owning_provider)
    : owning_provider_(*owning_provider) {
  // Note: has for non-optional scalars on proto3 messages would be implemented
  // as msg.value() != MessageType::default_instance.value(), but omited for
  // brevity.
  fields_["int64_value"] = std::make_unique<Field::FieldImpl<int64_t>>(
      &TestMessage::int64_value,
      /*has_fn=*/nullptr, &TestMessage::set_int64_value);
  fields_["double_value"] = std::make_unique<Field::FieldImpl<double>>(
      &TestMessage::double_value,
      /*has_fn=*/nullptr, &TestMessage::set_double_value);
  fields_["bool_value"] = std::make_unique<Field::FieldImpl<bool>>(
      &TestMessage::bool_value,
      /*has_fn=*/nullptr, &TestMessage::set_bool_value);
  fields_["int64_wrapper_value"] =
      std::make_unique<Field::FieldImpl<Int64Value>>(
          &TestMessage::int64_wrapper_value,
          &TestMessage::has_int64_wrapper_value,
          &TestMessage::set_allocated_int64_wrapper_value);
}

absl::StatusOr<CelValue::MessageWrapper::Builder> DemoTestMessage::NewInstance(
    cel::MemoryManagerRef memory_manager) const {
  auto* ts = google::protobuf::Arena::Create<TestMessage>(
      cel::extensions::ProtoMemoryManagerArena(memory_manager));
  return CelValue::MessageWrapper::Builder(ts);
}

absl::Status DemoTestMessage::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder& instance) const {
  auto iter = fields_.find(field_name);
  if (iter == fields_.end()) {
    return absl::UnknownError("no such field");
  }
  auto* mutable_test_msg =
      cel::internal::down_cast<TestMessage*>(instance.message_ptr());
  return iter->second->Set(mutable_test_msg, value);
}

absl::StatusOr<CelValue> DemoTestMessage::AdaptFromWellKnownType(
    cel::MemoryManagerRef memory_manager,
    CelValue::MessageWrapper::Builder instance) const {
  return CelValue::CreateMessageWrapper(
      instance.Build(owning_provider_.GetTypeInfoInstance()));
}

absl::StatusOr<bool> DemoTestMessage::HasField(
    absl::string_view field_name, const CelValue::MessageWrapper& value) const {
  auto iter = fields_.find(field_name);
  if (iter == fields_.end()) {
    return absl::UnknownError("no such field");
  }
  auto* test_msg =
      cel::internal::down_cast<const TestMessage*>(value.message_ptr());
  return iter->second->Has(test_msg);
}

// Access field on instance.
absl::StatusOr<CelValue> DemoTestMessage::GetField(
    absl::string_view field_name, const CelValue::MessageWrapper& instance,
    ProtoWrapperTypeOptions unboxing_option,
    cel::MemoryManagerRef memory_manager) const {
  auto iter = fields_.find(field_name);
  if (iter == fields_.end()) {
    return absl::UnknownError("no such field");
  }
  auto* test_msg =
      cel::internal::down_cast<const TestMessage*>(instance.message_ptr());
  return iter->second->Get(test_msg);
}

TEST(PortableCelExprBuilderFactoryTest, CreateNullOnMissingTypeProvider) {
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(nullptr);
  ASSERT_EQ(builder, nullptr);
}

TEST(PortableCelExprBuilderFactoryTest, CreateSuccess) {
  google::protobuf::Arena arena;

  InterpreterOptions opts;
  Activation activation;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::make_unique<DemoTypeProvider>(), opts);
  ASSERT_OK_AND_ASSIGN(
      ParsedExpr expr,
      parser::Parse("google.protobuf.Timestamp{seconds: 3000, nanos: 20}"));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  absl::Time result_time;
  ASSERT_TRUE(result.GetValue(&result_time));
  EXPECT_EQ(result_time,
            absl::UnixEpoch() + absl::Minutes(50) + absl::Nanoseconds(20));
}

TEST(PortableCelExprBuilderFactoryTest, CreateCustomMessage) {
  google::protobuf::Arena arena;

  InterpreterOptions opts;
  Activation activation;
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::make_unique<DemoTypeProvider>(), opts);
  ASSERT_OK_AND_ASSIGN(
      ParsedExpr expr,
      parser::Parse("google.api.expr.runtime.TestMessage{int64_value: 20, "
                    "double_value: 3.5}.double_value"));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), opts));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));

  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  double result_double;
  ASSERT_TRUE(result.GetValue(&result_double)) << result.DebugString();
  EXPECT_EQ(result_double, 3.5);
}

TEST(PortableCelExprBuilderFactoryTest, ActivationAndCreate) {
  google::protobuf::Arena arena;

  InterpreterOptions opts;
  Activation activation;
  auto provider = std::make_unique<DemoTypeProvider>();
  auto* provider_view = provider.get();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::move(provider), opts);
  builder->set_container("google.api.expr.runtime");
  ASSERT_OK_AND_ASSIGN(
      ParsedExpr expr,
      parser::Parse("TestMessage{int64_value: 20, bool_value: "
                    "false}.bool_value || my_var.bool_value ? 1 : 2"));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), opts));

  ASSERT_OK_AND_ASSIGN(
      auto plan, builder->CreateExpression(&expr.expr(), &expr.source_info()));
  TestMessage my_var;
  my_var.set_bool_value(true);
  activation.InsertValue("my_var", provider_view->WrapValue(&my_var));
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  int64_t result_int64;
  ASSERT_TRUE(result.GetValue(&result_int64)) << result.DebugString();
  EXPECT_EQ(result_int64, 1);
}

TEST(PortableCelExprBuilderFactoryTest, WrapperTypes) {
  google::protobuf::Arena arena;
  InterpreterOptions opts;
  opts.enable_heterogeneous_equality = true;
  Activation activation;
  auto provider = std::make_unique<DemoTypeProvider>();
  const auto* provider_view = provider.get();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::move(provider), opts);
  builder->set_container("google.api.expr.runtime");
  ASSERT_OK_AND_ASSIGN(ParsedExpr null_expr,
                       parser::Parse("my_var.int64_wrapper_value != null ? "
                                     "my_var.int64_wrapper_value > 29 : null"));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), opts));
  TestMessage my_var;
  my_var.set_bool_value(true);
  activation.InsertValue("my_var", provider_view->WrapValue(&my_var));

  ASSERT_OK_AND_ASSIGN(
      auto plan,
      builder->CreateExpression(&null_expr.expr(), &null_expr.source_info()));
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  EXPECT_TRUE(result.IsNull()) << result.DebugString();

  my_var.mutable_int64_wrapper_value()->set_value(30);

  ASSERT_OK_AND_ASSIGN(result, plan->Evaluate(activation, &arena));
  bool result_bool;
  ASSERT_TRUE(result.GetValue(&result_bool)) << result.DebugString();
  EXPECT_TRUE(result_bool);
}

TEST(PortableCelExprBuilderFactoryTest, SimpleBuiltinFunctions) {
  google::protobuf::Arena arena;
  InterpreterOptions opts;
  opts.enable_heterogeneous_equality = true;
  Activation activation;
  auto provider = std::make_unique<DemoTypeProvider>();
  std::unique_ptr<CelExpressionBuilder> builder =
      CreatePortableExprBuilder(std::move(provider), opts);
  builder->set_container("google.api.expr.runtime");

  // Fairly complicated but silly expression to cover a mix of builtins
  // (comparisons, arithmetic, datetime).
  ASSERT_OK_AND_ASSIGN(
      ParsedExpr ternary_expr,
      parser::Parse(
          "TestMessage{int64_value: 2}.int64_value + 1 < "
          "  TestMessage{double_value: 3.5}.double_value - 0.1 ? "
          "    (google.protobuf.Timestamp{seconds: 300} - timestamp(240) "
          "      >= duration('1m')  ? 'yes' : 'no') :"
          "    null"));
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), opts));

  ASSERT_OK_AND_ASSIGN(auto plan,
                       builder->CreateExpression(&ternary_expr.expr(),
                                                 &ternary_expr.source_info()));
  ASSERT_OK_AND_ASSIGN(CelValue result, plan->Evaluate(activation, &arena));

  ASSERT_TRUE(result.IsString()) << result.DebugString();
  EXPECT_EQ(result.StringOrDie().value(), "yes");
}

}  // namespace
}  // namespace google::api::expr::runtime
