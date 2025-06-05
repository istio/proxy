// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// General benchmarks for CEL evaluator.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "eval/tests/request_context.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/protobuf/value.h"
#include "internal/benchmark.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/managed_value_factory.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(bool, enable_recursive_planning, false, "enable recursive planning");
ABSL_FLAG(bool, enable_ref_counting, false,
          "enable reference counting memory management");

namespace cel {

namespace {

using ::absl_testing::IsOkAndHolds;
using ::cel::extensions::ProtobufRuntimeAdapter;
using ::cel::extensions::ProtoMemoryManagerRef;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::RequestContext;
using ::google::rpc::context::AttributeContext;

RuntimeOptions GetOptions() {
  RuntimeOptions options;

  if (absl::GetFlag(FLAGS_enable_recursive_planning)) {
    options.max_recursion_depth = -1;
  }

  return options;
}

enum class ConstFoldingEnabled { kNo, kYes };

std::unique_ptr<const cel::Runtime> StandardRuntimeOrDie(
    const cel::RuntimeOptions& options, google::protobuf::Arena* arena = nullptr,
    ConstFoldingEnabled const_folding = ConstFoldingEnabled::kNo) {
  auto builder = CreateStandardRuntimeBuilder(
      internal::GetTestingDescriptorPool(), options);
  ABSL_CHECK_OK(builder.status());

  switch (const_folding) {
    case ConstFoldingEnabled::kNo:
      break;
    case ConstFoldingEnabled::kYes:
      ABSL_CHECK(arena != nullptr);
      ABSL_CHECK_OK(extensions::EnableConstantFolding(
          *builder, ProtoMemoryManagerRef(arena)));
      break;
  }

  auto runtime = std::move(builder).value().Build();
  ABSL_CHECK_OK(runtime.status());
  return std::move(runtime).value();
}

// Set the appropriate memory manager for based on flags.
MemoryManagerRef GetMemoryManagerForBenchmark(google::protobuf::Arena* arena) {
  if (absl::GetFlag(FLAGS_enable_ref_counting)) {
    return MemoryManagerRef::ReferenceCounting();
  } else {
    return ProtoMemoryManagerRef(arena);
  }
}

template <typename T>
Value WrapMessageOrDie(ValueManager& value_manager, const T& message) {
  auto value = extensions::ProtoMessageToValue(value_manager, message);
  ABSL_CHECK_OK(value.status());
  return std::move(value).value();
}

// Benchmark test
// Evaluates cel expression:
// '1 + 1 + 1 .... +1'
static void BM_Eval(benchmark::State& state) {
  google::protobuf::Arena arena;
  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_int64_value(1);
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_int64_value(1);

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, root_expr));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                      GetMemoryManagerForBenchmark(&arena));
    Activation activation;
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_TRUE(Cast<IntValue>(result) == len + 1);
  }
}

BENCHMARK(BM_Eval)->Range(1, 10000);

absl::Status EmptyCallback(int64_t expr_id, const Value&, ValueManager&) {
  return absl::OkStatus();
}

// Benchmark test
// Traces cel expression with an empty callback:
// '1 + 1 + 1 .... +1'
static void BM_Eval_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  RuntimeOptions options = GetOptions();
  options.enable_recursive_tracing = true;

  auto runtime = StandardRuntimeOrDie(options);

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_int64_value(1);
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_int64_value(1);

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, root_expr));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    cel::ManagedValueFactory value_factory(
        runtime->GetTypeProvider(), GetMemoryManagerForBenchmark(&arena));
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        cel_expr->Trace(activation, EmptyCallback, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_TRUE(Cast<IntValue>(result) == len + 1);
  }
}

// A number higher than 10k leads to a stack overflow due to the recursive
// nature of the proto to native type conversion.
BENCHMARK(BM_Eval_Trace)->Range(1, 10000);

// Benchmark test
// Evaluates cel expression:
// '"a" + "a" + "a" .... + "a"'
static void BM_EvalString(benchmark::State& state) {
  google::protobuf::Arena arena;
  RuntimeOptions options = GetOptions();

  auto runtime = StandardRuntimeOrDie(options);

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_string_value("a");
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_string_value("a");

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, root_expr));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    cel::ManagedValueFactory value_factory(
        runtime->GetTypeProvider(), GetMemoryManagerForBenchmark(&arena));
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<StringValue>(result));
    ASSERT_TRUE(Cast<StringValue>(result).Size() == len + 1);
  }
}

// A number higher than 10k leads to a stack overflow due to the recursive
// nature of the proto to native type conversion.
BENCHMARK(BM_EvalString)->Range(1, 10000);

// Benchmark test
// Traces cel expression with an empty callback:
// '"a" + "a" + "a" .... + "a"'
static void BM_EvalString_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  RuntimeOptions options = GetOptions();
  options.enable_recursive_tracing = true;

  auto runtime = StandardRuntimeOrDie(options);

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_string_value("a");
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_string_value("a");

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, root_expr));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    cel::ManagedValueFactory value_factory(
        runtime->GetTypeProvider(), GetMemoryManagerForBenchmark(&arena));
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        cel_expr->Trace(activation, EmptyCallback, value_factory.get()));
    ASSERT_TRUE(InstanceOf<StringValue>(result));
    ASSERT_TRUE(Cast<StringValue>(result).Size() == len + 1);
  }
}

// A number higher than 10k leads to a stack overflow due to the recursive
// nature of the proto to native type conversion.
BENCHMARK(BM_EvalString_Trace)->Range(1, 10000);

const char kIP[] = "10.0.1.2";
const char kPath[] = "/admin/edit";
const char kToken[] = "admin";

ABSL_ATTRIBUTE_NOINLINE
bool NativeCheck(absl::btree_map<std::string, std::string>& attributes,
                 const absl::flat_hash_set<std::string>& denylists,
                 const absl::flat_hash_set<std::string>& allowlists) {
  auto& ip = attributes["ip"];
  auto& path = attributes["path"];
  auto& token = attributes["token"];
  if (denylists.find(ip) != denylists.end()) {
    return false;
  }
  if (absl::StartsWith(path, "v1")) {
    if (token == "v1" || token == "v2" || token == "admin") {
      return true;
    }
  } else if (absl::StartsWith(path, "v2")) {
    if (token == "v2" || token == "admin") {
      return true;
    }
  } else if (absl::StartsWith(path, "/admin")) {
    if (token == "admin") {
      if (allowlists.find(ip) != allowlists.end()) {
        return true;
      }
    }
  }
  return false;
}

void BM_PolicyNative(benchmark::State& state) {
  const auto denylists =
      absl::flat_hash_set<std::string>{"10.0.1.4", "10.0.1.5", "10.0.1.6"};
  const auto allowlists =
      absl::flat_hash_set<std::string>{"10.0.1.1", "10.0.1.2", "10.0.1.3"};
  auto attributes = absl::btree_map<std::string, std::string>{
      {"ip", kIP}, {"token", kToken}, {"path", kPath}};
  for (auto _ : state) {
    auto result = NativeCheck(attributes, denylists, allowlists);
    ASSERT_TRUE(result);
  }
}

BENCHMARK(BM_PolicyNative);

void BM_PolicySymbolic(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
   !(ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((path.startsWith("v1") && token in ["v1", "v2", "admin"]) ||
    (path.startsWith("v2") && token in ["v2", "admin"]) ||
    (path.startsWith("/admin") && token == "admin" && ip in [
       "10.0.1.1",  "10.0.1.2", "10.0.1.3"
    ])
   ))cel"));

  RuntimeOptions options = GetOptions();
  auto runtime =
      StandardRuntimeOrDie(options, &arena, ConstFoldingEnabled::kYes);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));
  activation.InsertOrAssignValue(
      "ip", value_factory.get().CreateUncheckedStringValue(kIP));
  activation.InsertOrAssignValue(
      "path", value_factory.get().CreateUncheckedStringValue(kPath));
  activation.InsertOrAssignValue(
      "token", value_factory.get().CreateUncheckedStringValue(kToken));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    auto result_bool = As<BoolValue>(result);
    ASSERT_TRUE(result_bool && result_bool->NativeValue());
  }
}

BENCHMARK(BM_PolicySymbolic);

class RequestMapImpl : public ParsedMapValueInterface {
 public:
  size_t Size() const override { return 3; }

  absl::Status ListKeys(ValueManager& value_manager,
                        ListValue& result
                            ABSL_ATTRIBUTE_LIFETIME_BOUND) const override {
    return absl::UnimplementedError("Unsupported");
  }

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const override {
    return absl::UnimplementedError("Unsupported");
  }

  std::string DebugString() const override { return "RequestMapImpl"; }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const override {
    return absl::UnimplementedError("Unsupported");
  }

  ParsedMapValue Clone(ArenaAllocator<> allocator) const override {
    return ParsedMapValue(
        MemoryManager::Pooling(allocator.arena()).MakeShared<RequestMapImpl>());
  }

 protected:
  // Called by `Find` after performing various argument checks.
  absl::StatusOr<bool> FindImpl(
      ValueManager& value_manager, const Value& key,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const override {
    auto string_value = As<StringValue>(key);
    if (!string_value) {
      return false;
    }
    if (string_value->Equals("ip")) {
      scratch = value_manager.CreateUncheckedStringValue(kIP);
    } else if (string_value->Equals("path")) {
      scratch = value_manager.CreateUncheckedStringValue(kPath);
    } else if (string_value->Equals("token")) {
      scratch = value_manager.CreateUncheckedStringValue(kToken);
    } else {
      return false;
    }
    return true;
  }

  // Called by `Has` after performing various argument checks.
  absl::StatusOr<bool> HasImpl(ValueManager& value_manager,
                               const Value& key) const override {
    return absl::UnimplementedError("Unsupported.");
  }

 private:
  NativeTypeId GetNativeTypeId() const override {
    return NativeTypeId::For<RequestMapImpl>();
  }
};

// Uses a lazily constructed map container for "ip", "path", and "token".
void BM_PolicySymbolicMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  RuntimeOptions options = GetOptions();

  auto runtime = StandardRuntimeOrDie(options);

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));
  ParsedMapValue map_value(
      value_factory.get().GetMemoryManager().MakeShared<RequestMapImpl>());

  activation.InsertOrAssignValue("request", std::move(map_value));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_PolicySymbolicMap);

// Uses a protobuf container for "ip", "path", and "token".
void BM_PolicySymbolicProto(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  RuntimeOptions options = GetOptions();

  auto runtime = StandardRuntimeOrDie(options);

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));
  Activation activation;
  RequestContext request;
  request.set_ip(kIP);
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_PolicySymbolicProto);

// This expression has no equivalent CEL
constexpr char kListSum[] = R"(
id: 1
comprehension_expr: <
  accu_var: "__result__"
  iter_var: "x"
  iter_range: <
    id: 2
    ident_expr: <
      name: "list_var"
    >
  >
  accu_init: <
    id: 3
    const_expr: <
      int64_value: 0
    >
  >
  loop_step: <
    id: 4
    call_expr: <
      function: "_+_"
      args: <
        id: 5
        ident_expr: <
          name: "__result__"
        >
      >
      args: <
        id: 6
        ident_expr: <
          name: "x"
        >
      >
    >
  >
  loop_condition: <
    id: 7
    const_expr: <
      bool_value: true
    >
  >
  result: <
    id: 8
    ident_expr: <
      name: "__result__"
    >
  >
>)";

void BM_Comprehension(benchmark::State& state) {
  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  auto runtime = StandardRuntimeOrDie(options);

  Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListSum, &expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_EQ(Cast<IntValue>(result), len);
  }
}

BENCHMARK(BM_Comprehension)->Range(1, 1 << 20);

void BM_Comprehension_Trace(benchmark::State& state) {
  RuntimeOptions options = GetOptions();
  options.enable_recursive_tracing = true;

  options.comprehension_max_iterations = 10000000;
  auto runtime = StandardRuntimeOrDie(options);
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListSum, &expr));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }
  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        cel_expr->Trace(activation, EmptyCallback, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_EQ(Cast<IntValue>(result), len);
  }
}

BENCHMARK(BM_Comprehension_Trace)->Range(1, 1 << 20);

void BM_HasMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       Parse("has(request.path) && !has(request.ip)"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(auto map_builder, value_factory.get().NewMapValueBuilder(
                                             cel::JsonMapType()));

  ASSERT_OK(
      map_builder->Put(value_factory.get().CreateUncheckedStringValue("path"),
                       value_factory.get().CreateUncheckedStringValue("path")));

  activation.InsertOrAssignValue("request", std::move(*map_builder).Build());

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_HasMap);

void BM_HasProto(benchmark::State& state) {
  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       Parse("has(request.path) && !has(request.ip)"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  RequestContext request;
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_HasProto);

void BM_HasProtoMap(benchmark::State& state) {
  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       Parse("has(request.headers.create_time) && "
                             "!has(request.headers.update_time)"));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_HasProtoMap);

void BM_ReadProtoMap(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
     request.headers.create_time == "2021-01-01"
   )cel"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_ReadProtoMap);

void BM_NestedProtoFieldRead(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
      !request.a.b.c.d.e
   )cel"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  RequestContext request;
  request.mutable_a()->mutable_b()->mutable_c()->mutable_d()->set_e(false);
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_NestedProtoFieldRead);

void BM_NestedProtoFieldReadDefaults(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
      !request.a.b.c.d.e
   )cel"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  RequestContext request;
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_NestedProtoFieldReadDefaults);

void BM_ProtoStructAccess(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
      has(request.auth.claims.iss) && request.auth.claims.iss == 'accounts.google.com'
   )cel"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  AttributeContext::Request request;
  auto* auth = request.mutable_auth();
  (*auth->mutable_claims()->mutable_fields())["iss"].set_string_value(
      "accounts.google.com");
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_ProtoStructAccess);

void BM_ProtoListAccess(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(R"cel(
      "//.../accessLevels/MY_LEVEL_4" in request.auth.access_levels
   )cel"));

  RuntimeOptions options = GetOptions();
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  AttributeContext::Request request;
  auto* auth = request.mutable_auth();
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_0");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_1");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_2");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_3");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_4");
  activation.InsertOrAssignValue(
      "request", WrapMessageOrDie(value_factory.get(), request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<BoolValue>(result) &&
                Cast<BoolValue>(result).NativeValue());
  }
}

BENCHMARK(BM_ProtoListAccess);

// This expression has no equivalent CEL expression.
// Sum a square with a nested comprehension
constexpr char kNestedListSum[] = R"(
id: 1
comprehension_expr: <
  accu_var: "__result__"
  iter_var: "x"
  iter_range: <
    id: 2
    ident_expr: <
      name: "list_var"
    >
  >
  accu_init: <
    id: 3
    const_expr: <
      int64_value: 0
    >
  >
  loop_step: <
    id: 4
    call_expr: <
      function: "_+_"
      args: <
        id: 5
        ident_expr: <
          name: "__result__"
        >
      >
      args: <
        id: 6
        comprehension_expr: <
          accu_var: "__result__"
          iter_var: "x"
          iter_range: <
            id: 9
            ident_expr: <
              name: "list_var"
            >
          >
          accu_init: <
            id: 10
            const_expr: <
              int64_value: 0
            >
          >
          loop_step: <
            id: 11
            call_expr: <
              function: "_+_"
              args: <
                id: 12
                ident_expr: <
                  name: "__result__"
                >
              >
              args: <
                id: 13
                ident_expr: <
                  name: "x"
                >
              >
            >
          >
          loop_condition: <
            id: 14
            const_expr: <
              bool_value: true
            >
          >
          result: <
            id: 15
            ident_expr: <
              name: "__result__"
            >
          >
        >
      >
    >
  >
  loop_condition: <
    id: 7
    const_expr: <
      bool_value: true
    >
  >
  result: <
    id: 8
    ident_expr: <
      name: "__result__"
    >
  >
>)";

void BM_NestedComprehension(benchmark::State& state) {
  Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedListSum, &expr));

  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  auto runtime = StandardRuntimeOrDie(options);

  google::protobuf::Arena arena;
  Activation activation;
  cel::ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                         GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_EQ(Cast<IntValue>(result), len * len);
  }
}

BENCHMARK(BM_NestedComprehension)->Range(1, 1 << 10);

void BM_NestedComprehension_Trace(benchmark::State& state) {
  Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedListSum, &expr));

  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  options.enable_recursive_tracing = true;

  auto runtime = StandardRuntimeOrDie(options);

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       ProtobufRuntimeAdapter::CreateProgram(*runtime, expr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        cel_expr->Trace(activation, &EmptyCallback, value_factory.get()));
    ASSERT_TRUE(InstanceOf<IntValue>(result));
    ASSERT_EQ(Cast<IntValue>(result), len * len);
  }
}

BENCHMARK(BM_NestedComprehension_Trace)->Range(1, 1 << 10);

void BM_ListComprehension(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("list_var.map(x, x * 2)"));

  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  auto runtime = StandardRuntimeOrDie(options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  google::protobuf::Arena arena;
  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<ListValue>(result));
    ASSERT_THAT(Cast<ListValue>(result).Size(), IsOkAndHolds(len));
  }
}

BENCHMARK(BM_ListComprehension)->Range(1, 1 << 16);

void BM_ListComprehension_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("list_var.map(x, x * 2)"));

  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  options.enable_recursive_tracing = true;

  auto runtime = StandardRuntimeOrDie(options);
  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        cel::Value result,
        cel_expr->Trace(activation, EmptyCallback, value_factory.get()));
    ASSERT_TRUE(InstanceOf<ListValue>(result));
    ASSERT_THAT(Cast<ListValue>(result).Size(), IsOkAndHolds(len));
  }
}

BENCHMARK(BM_ListComprehension_Trace)->Range(1, 1 << 16);

void BM_ListComprehension_Opt(benchmark::State& state) {
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse("list_var.map(x, x * 2)"));

  RuntimeOptions options = GetOptions();
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  auto runtime =
      StandardRuntimeOrDie(options, &arena, ConstFoldingEnabled::kYes);

  Activation activation;
  ManagedValueFactory value_factory(runtime->GetTypeProvider(),
                                    GetMemoryManagerForBenchmark(&arena));

  ASSERT_OK_AND_ASSIGN(
      auto list_builder,
      value_factory.get().NewListValueBuilder(cel::ListType()));

  int len = state.range(0);
  list_builder->Reserve(len);
  for (int i = 0; i < len; i++) {
    ASSERT_OK(list_builder->Add(IntValue(1)));
  }

  activation.InsertOrAssignValue("list_var", std::move(*list_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto cel_expr, ProtobufRuntimeAdapter::CreateProgram(
                                          *runtime, parsed_expr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         cel_expr->Evaluate(activation, value_factory.get()));
    ASSERT_TRUE(InstanceOf<ListValue>(result));
    ASSERT_THAT(Cast<ListValue>(result).Size(), IsOkAndHolds(len));
  }
}

BENCHMARK(BM_ListComprehension_Opt)->Range(1, 1 << 16);

void BM_ComprehensionCpp(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;

  std::vector<Value> list;

  int len = state.range(0);
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(IntValue(1));
  }

  auto op = [&list]() {
    int sum = 0;
    for (const auto& value : list) {
      sum += Cast<IntValue>(value).NativeValue();
    }
    return sum;
  };
  for (auto _ : state) {
    int result = op();
    ASSERT_EQ(result, len);
  }
}

BENCHMARK(BM_ComprehensionCpp)->Range(1, 1 << 20);

}  // namespace

}  // namespace cel
