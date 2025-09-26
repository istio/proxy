#include "quiche/blind_sign_auth/blind_sign_auth_test_data.h"

#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche::test {
namespace {

TEST(BlindSignAuthTestDataTest, CreateTestData) {
  QUICHE_EXPECT_OK(BlindSignAuthTestData::Create());
}

}  // namespace
}  // namespace quiche::test
