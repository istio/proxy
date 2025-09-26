#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_

#include "eval/public/activation.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

enum class ProtoUnsetFieldOptions {
  // Do not bind a field if it is unset. Repeated fields are bound as empty
  // list.
  kSkip,
  // Bind the (cc api) default value for a field.
  kBindDefault
};

// Utility method, that takes a protobuf Message and interprets it as a
// namespace, binding its fields to Activation. |arena| must be non-null.
//
// Field names and values become respective names and values of parameters
// bound to the Activation object.
// Example:
// Assume we have a protobuf message of type:
// message Person {
//   int age = 1;
//   string name = 2;
// }
//
// The sample code snippet will look as follows:
//
//   Person person;
//   person.set_name("John Doe");
//   person.age(42);
//
//   CEL_RETURN_IF_ERROR(BindProtoToActivation(&person, &arena, &activation));
//
// After this snippet, activation will have two parameters bound:
//  "name", with string value of "John Doe"
//  "age", with int value of 42.
//
// The default behavior for unset fields is to skip them. E.g. if the name field
// is not set on the Person message, it will not be bound in to the activation.
// ProtoUnsetFieldOptions::kBindDefault, will bind the cc proto api default for
// the field (either an explicit default value or a type specific default).
//
// TODO(issues/41): Consider updating the default behavior to bind default
// values for unset fields.
absl::Status BindProtoToActivation(
    const google::protobuf::Message* message, google::protobuf::Arena* arena,
    Activation* activation,
    ProtoUnsetFieldOptions options = ProtoUnsetFieldOptions::kSkip);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_ACTIVATION_BIND_HELPER_H_
