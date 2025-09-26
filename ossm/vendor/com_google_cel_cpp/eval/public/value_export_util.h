#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_

#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

// Exports content of CelValue as google.protobuf.Value.
// Current limitations:
// - exports integer values as doubles (Value.number_value);
// - exports integer keys in maps as strings;
// - handles Duration and Timestamp as generic messages.
absl::Status ExportAsProtoValue(const CelValue& in_value,
                                google::protobuf::Value* out_value,
                                google::protobuf::Arena* arena);

inline absl::Status ExportAsProtoValue(const CelValue& in_value,
                                       google::protobuf::Value* out_value) {
  google::protobuf::Arena arena;
  return ExportAsProtoValue(in_value, out_value, &arena);
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_VALUE_EXPORT_UTIL_H_
