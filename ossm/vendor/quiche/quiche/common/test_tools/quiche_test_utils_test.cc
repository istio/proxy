#include "quiche/common/test_tools/quiche_test_utils.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "quiche/common/platform/api/quiche_test.h"

namespace quiche::test {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

TEST(QuicheTestUtilsTest, StatusMatchers) {
  const absl::Status ok = absl::OkStatus();
  QUICHE_EXPECT_OK(ok);
  QUICHE_ASSERT_OK(ok);
  EXPECT_THAT(ok, IsOk());

  const absl::StatusOr<int> ok_with_value = 2023;
  QUICHE_EXPECT_OK(ok_with_value);
  QUICHE_ASSERT_OK(ok_with_value);
  EXPECT_THAT(ok_with_value, IsOk());
  EXPECT_THAT(ok_with_value, IsOkAndHolds(2023));

  const absl::Status err = absl::InternalError("test error");
  EXPECT_THAT(err, Not(IsOk()));
  EXPECT_THAT(err, StatusIs(absl::StatusCode::kInternal, HasSubstr("test")));

  const absl::StatusOr<int> err_with_value = absl::InternalError("test error");
  EXPECT_THAT(err_with_value, Not(IsOk()));
  EXPECT_THAT(err_with_value, Not(IsOkAndHolds(2023)));
  EXPECT_THAT(err_with_value,
              StatusIs(absl::StatusCode::kInternal, HasSubstr("test")));
}

}  // namespace
}  // namespace quiche::test
