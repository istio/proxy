#include "grpc_transcoding/status_error_listener.h"

#include <string>

namespace google {
namespace grpc {

namespace transcoding {

void StatusErrorListener::InvalidName(
    const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
    absl::string_view unknown_name, absl::string_view message) {
  status_ = absl::Status(absl::StatusCode::kInvalidArgument,
                         loc.ToString() + ": " + std::string(message));
}

void StatusErrorListener::InvalidValue(
    const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
    absl::string_view type_name, absl::string_view value) {
  status_ =
      absl::Status(absl::StatusCode::kInvalidArgument,
                   loc.ToString() + ": invalid value " + std::string(value) +
                       " for type " + std::string(type_name));
}

void StatusErrorListener::MissingField(
    const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
    absl::string_view missing_name) {
  status_ = absl::Status(
      absl::StatusCode::kInvalidArgument,
      loc.ToString() + ": missing field " + std::string(missing_name));
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google