#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_

#include "google/api/expr/v1alpha1/value.pb.h"
#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::api::expr::v1alpha1::Value;

// Translates a CelValue into a google::api::expr::v1alpha1::Value. Returns an error if
// translation is not supported.
absl::Status CelValueToValue(const CelValue& value, Value* result,
                             google::protobuf::Arena* arena);

inline absl::Status CelValueToValue(const CelValue& value, Value* result) {
  google::protobuf::Arena arena;
  return CelValueToValue(value, result, &arena);
}

// Translates a google::api::expr::v1alpha1::Value into a CelValue. Allocates any required
// external data on the provided arena. Returns an error if translation is not
// supported.
absl::StatusOr<CelValue> ValueToCelValue(const Value& value,
                                         google::protobuf::Arena* arena);


}  // namespace runtime

}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_TRANSFORM_UTILITY_H_
