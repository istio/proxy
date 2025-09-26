#include "eval/public/activation_bind_helper.h"

#include "absl/status/status.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/containers/field_backed_list_impl.h"
#include "eval/public/containers/field_backed_map_impl.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::protobuf::Arena;
using google::protobuf::Message;
using google::protobuf::FieldDescriptor;
using google::protobuf::Descriptor;

absl::Status CreateValueFromField(const google::protobuf::Message* msg,
                                  const FieldDescriptor* field_desc,
                                  google::protobuf::Arena* arena, CelValue* result) {
  if (field_desc->is_map()) {
    *result = CelValue::CreateMap(google::protobuf::Arena::Create<FieldBackedMapImpl>(
        arena, msg, field_desc, arena));
    return absl::OkStatus();
  } else if (field_desc->is_repeated()) {
    *result = CelValue::CreateList(google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, msg, field_desc, arena));
    return absl::OkStatus();
  } else {
    return CreateValueFromSingleField(msg, field_desc, arena, result);
  }
}

}  // namespace

absl::Status BindProtoToActivation(const Message* message, Arena* arena,
                                   Activation* activation,
                                   ProtoUnsetFieldOptions options) {
  // If we need to bind any types that are backed by an arena allocation, we
  // will cause a memory leak.
  if (arena == nullptr) {
    return absl::InvalidArgumentError(
        "arena must not be null for BindProtoToActivation.");
  }

  // TODO(issues/24): Improve the utilities to bind dynamic values as well.
  const Descriptor* desc = message->GetDescriptor();
  const google::protobuf::Reflection* reflection = message->GetReflection();
  for (int i = 0; i < desc->field_count(); i++) {
    CelValue value;
    const FieldDescriptor* field_desc = desc->field(i);

    if (options == ProtoUnsetFieldOptions::kSkip) {
      if (!field_desc->is_repeated() &&
          !reflection->HasField(*message, field_desc)) {
        continue;
      }
    }

    auto status = CreateValueFromField(message, field_desc, arena, &value);
    if (!status.ok()) {
      return status;
    }

    activation->InsertValue(field_desc->name(), value);
  }

  return absl::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
