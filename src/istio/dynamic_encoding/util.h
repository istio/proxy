/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ISTIO_DYNAMIC_ENCODING_UTIL_H
#define ISTIO_DYNAMIC_ENCODING_UTIL_H

#include <string>

#include "message_encoder.h"

#include "absl/types/any.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/stubs/status.h"

namespace istio {
namespace dynamic_encoding {
::google::protobuf::util::StatusOr<absl::any> GetEnumescriptorValue(
    absl::any* value,
    const ::google::protobuf::FieldDescriptor* field_descriptor);

::google::protobuf::util::Status EncodeStaticField(
    absl::any* value, MessageEncoder* msgEncoder,
    const ::google::protobuf::FieldDescriptor* field_descriptor, int index);

::google::protobuf::util::Status GetFieldEncodingError(
    const ::google::protobuf::FieldDescriptor* field_descriptor);

::google::protobuf::util::Status EncodeMessageField(
    absl::any* value, MessageEncoder* msgEncoder,
    const ::google::protobuf::FieldDescriptor* field_descriptor, int index);

}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_UTIL_H
