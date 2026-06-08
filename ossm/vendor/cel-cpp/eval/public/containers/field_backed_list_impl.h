#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_

#include "eval/public/cel_value.h"
#include "eval/public/containers/internal_field_backed_list_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelList implementation that uses "repeated" message field
// as backing storage.
class FieldBackedListImpl : public internal::FieldBackedListImpl {
 public:
  // message contains the "repeated" field
  // descriptor FieldDescriptor for the field
  // arena is used for incidental allocations when unwrapping the field.
  FieldBackedListImpl(const google::protobuf::Message* message,
                      const google::protobuf::FieldDescriptor* descriptor,
                      google::protobuf::Arena* arena)
      : internal::FieldBackedListImpl(
            message, descriptor, &CelProtoWrapper::InternalWrapMessage, arena) {
  }
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_CONTAINERS_FIELD_BACKED_LIST_IMPL_H_
