// Copyright 2024 Google LLC
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

// This file is a native C++ implementation of the original Go conformance test
// runner located at
// https://github.com/google/cel-spec/tree/master/tests/simple. It was ported to
// C++ to avoid having to pull in Go, gRPC, and others just to run C++
// conformance tests; as well as integrating better with C++ testing
// infrastructure.

#include <cstdlib>
#include <fstream>
#include <ios>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/checked.pb.h"
#include "google/api/expr/conformance/v1alpha1/conformance_service.pb.h"
#include "cel/expr/eval.pb.h"
#include "google/api/expr/v1alpha1/checked.pb.h"  // IWYU pragma: keep
#include "google/api/expr/v1alpha1/eval.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"  // IWYU pragma: keep
#include "google/api/expr/v1alpha1/value.pb.h"
#include "cel/expr/value.pb.h"
#include "google/rpc/code.pb.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "absl/types/span.h"
#include "conformance/service.h"
#include "conformance/utils.h"
#include "internal/testing.h"
#include "cel/expr/conformance/test/simple.pb.h"
#include "google/protobuf/io/zero_copy_stream_impl.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

ABSL_FLAG(bool, opt, false, "Enable optimizations (constant folding)");
ABSL_FLAG(
    bool, modern, false,
    "Use modern cel::Value APIs implementation of the conformance service.");
ABSL_FLAG(bool, recursive, false,
          "Enable recursive plans. Depth limited to slightly more than the "
          "default nesting limit.");
ABSL_FLAG(std::vector<std::string>, skip_tests, {}, "Tests to skip");
ABSL_FLAG(bool, dashboard, false, "Dashboard mode, ignore test failures");
ABSL_FLAG(bool, skip_check, true, "Skip type checking the expressions");

namespace {

using ::testing::IsEmpty;

using cel::expr::conformance::test::SimpleTest;
using cel::expr::conformance::test::SimpleTestFile;
using google::api::expr::conformance::v1alpha1::CheckRequest;
using google::api::expr::conformance::v1alpha1::CheckResponse;
using google::api::expr::conformance::v1alpha1::EvalRequest;
using google::api::expr::conformance::v1alpha1::EvalResponse;
using google::api::expr::conformance::v1alpha1::ParseRequest;
using google::api::expr::conformance::v1alpha1::ParseResponse;

google::rpc::Code ToGrpcCode(absl::StatusCode code) {
  return static_cast<google::rpc::Code>(code);
}

bool ShouldSkipTest(absl::Span<const std::string> tests_to_skip,
                    absl::string_view name) {
  for (absl::string_view test_to_skip : tests_to_skip) {
    auto consumed_name = name;
    if (absl::ConsumePrefix(&consumed_name, test_to_skip) &&
        (consumed_name.empty() || absl::StartsWith(consumed_name, "/"))) {
      return true;
    }
  }
  return false;
}

SimpleTest DefaultTestMatcherToTrueIfUnset(const SimpleTest& test) {
  auto test_copy = test;
  if (test_copy.result_matcher_case() == SimpleTest::RESULT_MATCHER_NOT_SET) {
    test_copy.mutable_value()->set_bool_value(true);
  }
  return test_copy;
}

class ConformanceTest : public testing::Test {
 public:
  explicit ConformanceTest(
      std::shared_ptr<cel_conformance::ConformanceServiceInterface> service,
      const SimpleTest& test, bool skip)
      : service_(std::move(service)),
        test_(DefaultTestMatcherToTrueIfUnset(test)),
        skip_(skip) {}

  void TestBody() override {
    if (skip_) {
      GTEST_SKIP();
    }
    ParseRequest parse_request;
    parse_request.set_cel_source(test_.expr());
    parse_request.set_source_location(test_.name());
    parse_request.set_disable_macros(test_.disable_macros());
    ParseResponse parse_response;
    service_->Parse(parse_request, parse_response);
    ASSERT_THAT(parse_response.issues(), IsEmpty());

    EvalRequest eval_request;
    if (!test_.container().empty()) {
      eval_request.set_container(test_.container());
    }
    if (!test_.bindings().empty()) {
      for (const auto& binding : test_.bindings()) {
        absl::Cord serialized;
        ABSL_CHECK(binding.second.SerializePartialToString(&serialized));
        ABSL_CHECK((*eval_request.mutable_bindings())[binding.first]
                       .ParsePartialFromString(serialized));
      }
    }

    if (absl::GetFlag(FLAGS_skip_check) || test_.disable_check()) {
      eval_request.set_allocated_parsed_expr(
          parse_response.release_parsed_expr());
    } else {
      CheckRequest check_request;
      check_request.set_allocated_parsed_expr(
          parse_response.release_parsed_expr());
      check_request.set_container(test_.container());
      for (const auto& type_env : test_.type_env()) {
        absl::Cord serialized;
        ABSL_CHECK(type_env.SerializePartialToString(&serialized));
        ABSL_CHECK(
            check_request.add_type_env()->ParsePartialFromString(serialized));
      }
      CheckResponse check_response;
      service_->Check(check_request, check_response);
      ASSERT_THAT(check_response.issues(), IsEmpty()) << absl::StrCat(
          "unexpected type check issues for: '", test_.expr(), "'\n");
      eval_request.set_allocated_checked_expr(
          check_response.release_checked_expr());
    }

    if (test_.check_only()) {
      ASSERT_TRUE(test_.has_typed_result())
          << "test must specify a typed result if check_only is set";
      EXPECT_THAT(eval_request.checked_expr(),
                  cel_conformance::ResultTypeMatches(
                      test_.typed_result().deduced_type()));
      return;
    }

    EvalResponse eval_response;
    if (auto status = service_->Eval(eval_request, eval_response);
        !status.ok()) {
      auto* issue = eval_response.add_issues();
      issue->set_message(status.message());
      issue->set_code(ToGrpcCode(status.code()));
    }
    ASSERT_TRUE(eval_response.has_result()) << eval_response;
    switch (test_.result_matcher_case()) {
      case SimpleTest::kValue: {
        absl::Cord serialized;
        ABSL_CHECK(
            eval_response.result().SerializePartialToString(&serialized));
        cel::expr::ExprValue test_value;
        ABSL_CHECK(test_value.ParsePartialFromString(serialized));
        EXPECT_THAT(test_value,
                    cel_conformance::MatchesConformanceValue(test_.value()));
        break;
      }
      case SimpleTest::kTypedResult: {
        ASSERT_TRUE(eval_request.has_checked_expr())
            << "expression was not type checked";
        absl::Cord serialized;
        ABSL_CHECK(
            eval_response.result().SerializePartialToString(&serialized));
        cel::expr::ExprValue test_value;
        ABSL_CHECK(test_value.ParsePartialFromString(serialized));
        EXPECT_THAT(test_value, cel_conformance::MatchesConformanceValue(
                                    test_.typed_result().result()));
        EXPECT_THAT(eval_request.checked_expr(),
                    cel_conformance::ResultTypeMatches(
                        test_.typed_result().deduced_type()));
        break;
      }
      case SimpleTest::kEvalError:
        EXPECT_TRUE(eval_response.result().has_error())
            << eval_response.result();
        break;
      default:
        ADD_FAILURE() << "unexpected matcher kind: "
                      << test_.result_matcher_case();
        break;
    }
  }

 private:
  const std::shared_ptr<cel_conformance::ConformanceServiceInterface> service_;
  const SimpleTest test_;
  const bool skip_;
};

absl::Status RegisterTestsFromFile(
    const std::shared_ptr<cel_conformance::ConformanceServiceInterface>&
        service,
    absl::Span<const std::string> tests_to_skip, absl::string_view path) {
  SimpleTestFile file;
  {
    std::ifstream in;
    in.open(std::string(path), std::ios_base::in | std::ios_base::binary);
    if (!in.is_open()) {
      return absl::UnknownError(absl::StrCat("failed to open file: ", path));
    }
    google::protobuf::io::IstreamInputStream stream(&in);
    if (!google::protobuf::TextFormat::Parse(&stream, &file)) {
      return absl::UnknownError(absl::StrCat("failed to parse file: ", path));
    }
  }
  for (const auto& section : file.section()) {
    for (const auto& test : section.test()) {
      const bool skip = ShouldSkipTest(
          tests_to_skip,
          absl::StrCat(file.name(), "/", section.name(), "/", test.name()));
      testing::RegisterTest(
          file.name().c_str(),
          absl::StrCat(section.name(), "/", test.name()).c_str(), nullptr,
          nullptr, __FILE__, __LINE__, [=]() -> ConformanceTest* {
            return new ConformanceTest(service, test, skip);
          });
    }
  }
  return absl::OkStatus();
}

// We could push this do be done per test or suite, but to avoid changing more
// than necessary we do it once to mimic the previous runner.
std::shared_ptr<cel_conformance::ConformanceServiceInterface>
NewConformanceServiceFromFlags() {
  auto status_or_service = cel_conformance::NewConformanceService(
      cel_conformance::ConformanceServiceOptions{
          .optimize = absl::GetFlag(FLAGS_opt),
          .modern = absl::GetFlag(FLAGS_modern),
          .recursive = absl::GetFlag(FLAGS_recursive)});
  ABSL_CHECK_OK(status_or_service);
  return std::shared_ptr<cel_conformance::ConformanceServiceInterface>(
      std::move(*status_or_service));
}

}  // namespace

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  {
    auto service = NewConformanceServiceFromFlags();
    auto tests_to_skip = absl::GetFlag(FLAGS_skip_tests);
    for (int argi = 1; argi < argc; argi++) {
      ABSL_CHECK_OK(RegisterTestsFromFile(service, tests_to_skip,
                                          absl::string_view(argv[argi])));
    }
  }
  int exit_code = RUN_ALL_TESTS();
  if (absl::GetFlag(FLAGS_dashboard)) {
    exit_code = EXIT_SUCCESS;
  }
  return exit_code;
}
