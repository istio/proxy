#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/statusor.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/internal_field_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"

namespace google::api::expr::runtime {

// CelMap implementation that uses "map" message field
// as backing storage.
//
// Trivial subclass of internal implementation to avoid API changes for clients
// that use this directly.
class FieldBackedMapImpl : public internal::FieldBackedMapImpl {
 public:
  // message contains the "map" field. Object stores the pointer
  // to the message, thus it is expected that message outlives the
  // object.
  // descriptor FieldDescriptor for the field
  // arena is used for incidental allocations from unpacking the field.
  FieldBackedMapImpl(const google::protobuf::Message* message,
                     const google::protobuf::FieldDescriptor* descriptor,
                     google::protobuf::Arena* arena)
      : internal::FieldBackedMapImpl(
            message, descriptor, &CelProtoWrapper::InternalWrapMessage, arena) {
  }
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_MAP_IMPL_H_
