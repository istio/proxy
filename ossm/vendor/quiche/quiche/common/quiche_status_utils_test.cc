// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/common/quiche_status_utils.h"

#include <optional>
#include <type_traits>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "quiche/common/platform/api/quiche_logging.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
namespace {

TEST(QuicheAssignOrReturnTest, HandlesValue) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN(int x, absl::StatusOr<int>(1));
    EXPECT_EQ(x, 1);
    return absl::OkStatus();
  };
  QUICHE_EXPECT_OK(TestLambda());
}

TEST(QuicheAssignOrReturnTest, HandlesValueAndDoesNotCallErrorLambda) {
  auto TestLambda = []() -> absl::Status {
    bool error_lambda_called = false;
    QUICHE_ASSIGN_OR_RETURN(int x, absl::StatusOr<int>(1),
                            [&](const absl::Status& status) {
                              error_lambda_called = true;
                              return status;
                            });
    EXPECT_EQ(x, 1);
    EXPECT_FALSE(error_lambda_called);
    return absl::OkStatus();
  };

  QUICHE_EXPECT_OK(TestLambda());
}

TEST(QuicheAssignOrReturnTest, HandlesError) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN([[maybe_unused]] int x,
                            absl::StatusOr<int>(absl::InternalError("error")));
    return absl::OkStatus();
  };
  EXPECT_THAT(TestLambda(),
              quiche::test::StatusIs(absl::StatusCode::kInternal, "error"));
}

TEST(QuicheAssignOrReturnTest, HandlesErrorAndCallsErrorLambda) {
  auto TestLambda = []() -> absl::Status {
    std::optional<absl::Status> captured_status;
    QUICHE_ASSIGN_OR_RETURN([[maybe_unused]] int x,
                            absl::StatusOr<int>(absl::InternalError("error")),
                            [&](const absl::Status& status) {
                              captured_status = status;
                              return status;
                            });
    EXPECT_THAT(captured_status, testing::Optional(quiche::test::StatusIs(
                                     absl::StatusCode::kInternal, "error")));
    return absl::OkStatus();
  };
  EXPECT_THAT(TestLambda(),
              quiche::test::StatusIs(absl::StatusCode::kInternal, "error"));
}

TEST(QuicheAssignOrReturnTest, HandlesErrorAndUsesReturnValueFromLambda) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN([[maybe_unused]] int x,
                            absl::StatusOr<int>(absl::InternalError("error")),
                            [](const absl::Status& status) {
                              return absl::InvalidArgumentError(absl::StrCat(
                                  "custom message: ", status.message()));
                            });
    return absl::OkStatus();
  };
  EXPECT_THAT(TestLambda(),
              quiche::test::StatusIs(absl::StatusCode::kInvalidArgument,
                                     "custom message: error"));
}

TEST(QuicheAssignOrReturnTest, CanBeUsedMultipleTimesInOneFunction) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN(int x, absl::StatusOr<int>(1));
    EXPECT_EQ(x, 1);
    QUICHE_ASSIGN_OR_RETURN(int y, absl::StatusOr<int>(2),
                            [](const absl::Status& status) { return status; });
    EXPECT_EQ(y, 2);
    QUICHE_ASSIGN_OR_RETURN([[maybe_unused]] int z,
                            absl::StatusOr<int>(absl::InternalError("error")));
    EXPECT_TRUE(false) << "Unreachable";
    return absl::OkStatus();
  };
  EXPECT_THAT(TestLambda(),
              quiche::test::StatusIs(absl::StatusCode::kInternal, "error"));
}

// Demonstrates that we can use `QUICHE_LOG` in an on-error lambda.
TEST(QuicheAssignOrReturnTest, CanLogOnError) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN([[maybe_unused]] int x,
                            absl::StatusOr<int>(absl::InternalError("error")),
                            [&](const absl::Status& status) {
                              QUICHE_LOG(INFO) << "Not OK! " << status;
                              return status;
                            });
    return absl::OkStatus();
  };
  EXPECT_THAT(TestLambda(),
              quiche::test::StatusIs(absl::StatusCode::kInternal, "error"));
}

struct FinickyNonCopyableInt {
  FinickyNonCopyableInt(int x) : value(x) {}
  FinickyNonCopyableInt(FinickyNonCopyableInt&&) = default;
  FinickyNonCopyableInt(FinickyNonCopyableInt&) = delete;

  int value;
};
static_assert(std::is_move_constructible_v<FinickyNonCopyableInt>);
static_assert(!std::is_copy_constructible_v<FinickyNonCopyableInt>);

TEST(QuicheAssignOrReturnTest, HandlesNonCopyableValue) {
  auto TestLambda = []() -> absl::Status {
    QUICHE_ASSIGN_OR_RETURN(
        FinickyNonCopyableInt non_copyable,
        absl::StatusOr<FinickyNonCopyableInt>(FinickyNonCopyableInt(42)));
    EXPECT_EQ(non_copyable.value, 42);
    return absl::OkStatus();
  };
  QUICHE_EXPECT_OK(TestLambda());
}

}  // namespace
}  // namespace quiche
