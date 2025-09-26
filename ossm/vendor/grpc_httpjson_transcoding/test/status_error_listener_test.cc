#include "grpc_transcoding/status_error_listener.h"

#include "gmock/gmock.h"
#include "google/protobuf/util/converter/object_location_tracker.h"
#include "gtest/gtest.h"

namespace google {
namespace grpc {

namespace transcoding {
namespace testing {
namespace {

using ::testing::HasSubstr;
using absl::Status;
using absl::StatusCode;
namespace pbconv = google::protobuf::util::converter;

class StatusErrorListenerTest : public ::testing::Test {
 protected:
  StatusErrorListenerTest() : listener_() {}

  StatusErrorListener listener_;
};

TEST_F(StatusErrorListenerTest, ReportFailures) {
  listener_.set_status(Status(StatusCode::kInvalidArgument, "invalid args"));
  EXPECT_EQ(listener_.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(listener_.status().ToString(), HasSubstr("invalid args"));

  listener_.InvalidName(pbconv::ObjectLocationTracker{}, "invalid name",
                        "invalid_name_foo");
  EXPECT_EQ(listener_.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(listener_.status().ToString(), HasSubstr("invalid_name_foo"));
  listener_.InvalidValue(pbconv::ObjectLocationTracker{}, "invalid value",
                         "invalid_value_foo");
  EXPECT_EQ(listener_.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(listener_.status().ToString(), HasSubstr("invalid_value_foo"));
  listener_.MissingField(pbconv::ObjectLocationTracker{}, "missing value");
  EXPECT_EQ(listener_.status().code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(listener_.status().ToString(),
              HasSubstr("missing field missing value"));
}

}  // namespace
}  // namespace testing
}  // namespace transcoding

}  // namespace grpc
}  // namespace google