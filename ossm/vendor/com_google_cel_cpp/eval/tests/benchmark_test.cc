#include "internal/benchmark.h"

#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/rpc/context/attribute_context.pb.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/tests/request_context.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(bool, enable_optimizations, false, "enable const folding opt");
ABSL_FLAG(bool, enable_recursive_planning, false, "enable recursive planning");

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::cel::expr::Expr;
using ::cel::expr::ParsedExpr;
using ::cel::expr::SourceInfo;
using ::google::rpc::context::AttributeContext;

InterpreterOptions GetOptions(google::protobuf::Arena& arena) {
  InterpreterOptions options;

  if (absl::GetFlag(FLAGS_enable_optimizations)) {
    options.constant_arena = &arena;
    options.constant_folding = true;
  }

  if (absl::GetFlag(FLAGS_enable_recursive_planning)) {
    options.max_recursion_depth = -1;
  }

  return options;
}

// Benchmark test
// Evaluates cel expression:
// '1 + 1 + 1 .... +1'
static void BM_Eval(benchmark::State& state) {
  google::protobuf::Arena arena;
  InterpreterOptions options = GetOptions(arena);

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

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
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_TRUE(result.Int64OrDie() == len + 1);
  }
}

BENCHMARK(BM_Eval)->Range(1, 10000);

absl::Status EmptyCallback(int64_t expr_id, const CelValue& value,
                           google::protobuf::Arena* arena) {
  return absl::OkStatus();
}

// Benchmark test
// Traces cel expression with an empty callback:
// '1 + 1 + 1 .... +1'
static void BM_Eval_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  InterpreterOptions options = GetOptions(arena);
  options.enable_recursive_tracing = true;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

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
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Trace(activation, &arena, EmptyCallback));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_TRUE(result.Int64OrDie() == len + 1);
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
  InterpreterOptions options = GetOptions(arena);

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

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
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsString());
    ASSERT_TRUE(result.StringOrDie().value().size() == len + 1);
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
  InterpreterOptions options = GetOptions(arena);
  options.enable_recursive_tracing = true;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

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
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Trace(activation, &arena, EmptyCallback));
    ASSERT_TRUE(result.IsString());
    ASSERT_TRUE(result.StringOrDie().value().size() == len + 1);
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
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
   !(ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((path.startsWith("v1") && token in ["v1", "v2", "admin"]) ||
    (path.startsWith("v2") && token in ["v2", "admin"]) ||
    (path.startsWith("/admin") && token == "admin" && ip in [
       "10.0.1.1",  "10.0.1.2", "10.0.1.3"
    ])
   ))cel"));

  InterpreterOptions options = GetOptions(arena);
  options.constant_folding = true;
  options.constant_arena = &arena;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &parsed_expr.expr(), &source_info));

  Activation activation;
  activation.InsertValue("ip", CelValue::CreateStringView(kIP));
  activation.InsertValue("path", CelValue::CreateStringView(kPath));
  activation.InsertValue("token", CelValue::CreateStringView(kToken));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_PolicySymbolic);

class RequestMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue key) const override {
    if (!key.IsString()) {
      return {};
    }
    auto value = key.StringOrDie().value();
    if (value == "ip") {
      return CelValue::CreateStringView(kIP);
    } else if (value == "path") {
      return CelValue::CreateStringView(kPath);
    } else if (value == "token") {
      return CelValue::CreateStringView(kToken);
    }
    return {};
  }
  int size() const override { return 3; }
  absl::StatusOr<const CelList*> ListKeys() const override {
    return absl::UnimplementedError("CelMap::ListKeys is not implemented");
  }
};

// Uses a lazily constructed map container for "ip", "path", and "token".
void BM_PolicySymbolicMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  InterpreterOptions options = GetOptions(arena);

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &parsed_expr.expr(), &source_info));

  Activation activation;
  RequestMap request;
  activation.InsertValue("request", CelValue::CreateMap(&request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_PolicySymbolicMap);

// Uses a protobuf container for "ip", "path", and "token".
void BM_PolicySymbolicProto(benchmark::State& state) {
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  InterpreterOptions options = GetOptions(arena);

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder->CreateExpression(
                                          &parsed_expr.expr(), &source_info));

  Activation activation;
  RequestContext request;
  request.set_ip(kIP);
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
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
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));

  InterpreterOptions options = GetOptions(arena);
  options.comprehension_max_iterations = 10000000;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len);
  }
}

BENCHMARK(BM_Comprehension)->Range(1, 1 << 20);

void BM_Comprehension_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options = GetOptions(arena);
  options.enable_recursive_tracing = true;

  options.comprehension_max_iterations = 10000000;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Trace(activation, &arena, EmptyCallback));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len);
  }
}

BENCHMARK(BM_Comprehension_Trace)->Range(1, 1 << 20);

void BM_HasMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("has(request.path) && !has(request.ip)"));

  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  std::vector<std::pair<CelValue, CelValue>> map_pairs{
      {CelValue::CreateStringView("path"), CelValue::CreateStringView("path")}};
  auto cel_map =
      CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          map_pairs.data(), map_pairs.size()));
  activation.InsertValue("request", CelValue::CreateMap((*cel_map).get()));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasMap);

void BM_HasProto(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("has(request.path) && !has(request.ip)"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry(), options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  RequestContext request;
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasProto);

void BM_HasProtoMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("has(request.headers.create_time) && "
                                     "!has(request.headers.update_time)"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry(), options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasProtoMap);

void BM_ReadProtoMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
     request.headers.create_time == "2021-01-01"
   )cel"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry(), options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_ReadProtoMap);

void BM_NestedProtoFieldRead(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
      !request.a.b.c.d.e
   )cel"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry(), options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  RequestContext request;
  request.mutable_a()->mutable_b()->mutable_c()->mutable_d()->set_e(false);
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_NestedProtoFieldRead);

void BM_NestedProtoFieldReadDefaults(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
      !request.a.b.c.d.e
   )cel"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry(), options);

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  RequestContext request;
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_NestedProtoFieldReadDefaults);

void BM_ProtoStructAccess(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
      has(request.auth.claims.iss) && request.auth.claims.iss == 'accounts.google.com'
   )cel"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  AttributeContext::Request request;
  auto* auth = request.mutable_auth();
  (*auth->mutable_claims()->mutable_fields())["iss"].set_string_value(
      "accounts.google.com");
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_ProtoStructAccess);

void BM_ProtoListAccess(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse(R"cel(
      "//.../accessLevels/MY_LEVEL_4" in request.auth.access_levels
   )cel"));
  InterpreterOptions options = GetOptions(arena);
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&parsed_expr.expr(), nullptr));

  AttributeContext::Request request;
  auto* auth = request.mutable_auth();
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_0");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_1");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_2");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_3");
  auth->add_access_levels("//.../accessLevels/MY_LEVEL_4");
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
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
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options = GetOptions(arena);
  options.comprehension_max_iterations = 10000000;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));

  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len * len);
  }
}

BENCHMARK(BM_NestedComprehension)->Range(1, 1 << 10);

void BM_NestedComprehension_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options = GetOptions(arena);
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  options.enable_recursive_tracing = true;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Trace(activation, &arena, EmptyCallback));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len * len);
  }
}

BENCHMARK(BM_NestedComprehension_Trace)->Range(1, 1 << 10);

void BM_ListComprehension(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("list_var.map(x, x * 2)"));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options = GetOptions(arena);
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder->CreateExpression(&(parsed_expr.expr()), nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsList());
    ASSERT_EQ(result.ListOrDie()->size(), len);
  }
}

BENCHMARK(BM_ListComprehension)->Range(1, 1 << 16);

void BM_ListComprehension_Trace(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("list_var.map(x, x * 2)"));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options = GetOptions(arena);
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  options.enable_recursive_tracing = true;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder->CreateExpression(&(parsed_expr.expr()), nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Trace(activation, &arena, EmptyCallback));
    ASSERT_TRUE(result.IsList());
    ASSERT_EQ(result.ListOrDie()->size(), len);
  }
}

BENCHMARK(BM_ListComprehension_Trace)->Range(1, 1 << 16);

void BM_ListComprehension_Opt(benchmark::State& state) {
  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("list_var.map(x, x * 2)"));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list_var", CelValue::CreateList(&cel_list));
  InterpreterOptions options;
  options.constant_arena = &arena;
  options.constant_folding = true;
  options.comprehension_max_iterations = 10000000;
  options.enable_comprehension_list_append = true;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder->CreateExpression(&(parsed_expr.expr()), nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsList());
    ASSERT_EQ(result.ListOrDie()->size(), len);
  }
}

BENCHMARK(BM_ListComprehension_Opt)->Range(1, 1 << 16);

void BM_ComprehensionCpp(benchmark::State& state) {
  Activation activation;

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  auto op = [&list]() {
    int sum = 0;
    for (const auto& value : list) {
      sum += value.Int64OrDie();
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

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
