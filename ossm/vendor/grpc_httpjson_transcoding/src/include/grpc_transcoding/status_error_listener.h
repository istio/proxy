#ifndef GRPC_TRANSCODING_STATUS_ERROR_LISTENER_H_
#define GRPC_TRANSCODING_STATUS_ERROR_LISTENER_H_

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/error_listener.h"

namespace google {
namespace grpc {

namespace transcoding {

// StatusErrorListener converts the error events into a Status
class StatusErrorListener
    : public ::google::protobuf::util::converter::ErrorListener {
 public:
  StatusErrorListener() {}
  virtual ~StatusErrorListener() {}

  absl::Status status() const { return status_; }

  // ErrorListener implementation
  void InvalidName(
      const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
      absl::string_view unknown_name, absl::string_view message);
  void InvalidValue(
      const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
      absl::string_view type_name, absl::string_view value);
  void MissingField(
      const ::google::protobuf::util::converter::LocationTrackerInterface& loc,
      absl::string_view missing_name);

  void set_status(absl::Status status) { status_ = status; }

 private:
  absl::Status status_;

  StatusErrorListener(const StatusErrorListener&) = delete;
  StatusErrorListener& operator=(const StatusErrorListener&) = delete;
};

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
#endif  // GRPC_TRANSCODING_STATUS_ERROR_LISTENER_H_
