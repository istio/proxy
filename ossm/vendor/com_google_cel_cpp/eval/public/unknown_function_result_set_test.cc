#include "eval/public/unknown_function_result_set.h"

#include <sys/ucontext.h>

#include <memory>
#include <string>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/arena.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/testing.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Arena;
using ::testing::Eq;
using ::testing::SizeIs;

CelFunctionDescriptor kTwoInt("TwoInt", false,
                              {CelValue::Type::kInt64, CelValue::Type::kInt64});

CelFunctionDescriptor kOneInt("OneInt", false, {CelValue::Type::kInt64});

TEST(UnknownFunctionResult, Equals) {
  UnknownFunctionResult call1(kTwoInt, /*expr_id=*/0);

  UnknownFunctionResult call2(kTwoInt, /*expr_id=*/0);

  EXPECT_TRUE(call1.IsEqualTo(call2));

  UnknownFunctionResult call3(kOneInt, /*expr_id=*/0);

  UnknownFunctionResult call4(kOneInt, /*expr_id=*/0);

  EXPECT_TRUE(call3.IsEqualTo(call4));

  UnknownFunctionResultSet call_set({call1, call3});
  EXPECT_EQ(call_set.size(), 2);
  EXPECT_EQ(*call_set.begin(), call3);
  EXPECT_EQ(*(++call_set.begin()), call1);
}

TEST(UnknownFunctionResult, InequalDescriptor) {
  UnknownFunctionResult call1(kTwoInt, /*expr_id=*/0);

  UnknownFunctionResult call2(kOneInt, /*expr_id=*/0);

  EXPECT_FALSE(call1.IsEqualTo(call2));

  CelFunctionDescriptor one_uint("OneInt", false, {CelValue::Type::kUint64});

  UnknownFunctionResult call3(kOneInt, /*expr_id=*/0);

  UnknownFunctionResult call4(one_uint, /*expr_id=*/0);

  EXPECT_FALSE(call3.IsEqualTo(call4));

  UnknownFunctionResultSet call_set({call1, call3, call4});
  EXPECT_EQ(call_set.size(), 3);
  auto it = call_set.begin();
  EXPECT_EQ(*it++, call3);
  EXPECT_EQ(*it++, call4);
  EXPECT_EQ(*it++, call1);
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
