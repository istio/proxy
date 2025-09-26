/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
// Internal helper class for type resolving. Note that this class is not
// thread-safe and should only be accessed in one thread.
class TypeInfo {
 public:
  TypeInfo() {}
  TypeInfo(const TypeInfo&) = delete;
  TypeInfo& operator=(const TypeInfo&) = delete;
  virtual ~TypeInfo() {}

  // Resolves a type url into a Type. If the type url is invalid, returns
  // INVALID_ARGUMENT error status. If the type url is valid but the
  // corresponding type cannot be found, returns a NOT_FOUND error status.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual absl::StatusOr<const google::protobuf::Type*> ResolveTypeUrl(
      absl::string_view type_url) const = 0;

  // Resolves a type url into a Type. Like ResolveTypeUrl() but returns
  // NULL if the type url is invalid or the type cannot be found.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual const google::protobuf::Type* GetTypeByTypeUrl(
      absl::string_view type_url) const = 0;

  // Resolves a type url for an enum. Returns NULL if the type url is
  // invalid or the type cannot be found.
  //
  // This TypeInfo class retains the ownership of the returned pointer.
  virtual const google::protobuf::Enum* GetEnumByTypeUrl(
      absl::string_view type_url) const = 0;

  // Looks up a field in the specified type given a CamelCase name.
  virtual const google::protobuf::Field* FindField(
      const google::protobuf::Type* type,
      absl::string_view camel_case_name) const = 0;

  // Creates a TypeInfo object that looks up type information from a
  // TypeResolver. Caller takes ownership of the returned pointer.
  static TypeInfo* NewTypeInfo(TypeResolver* type_resolver);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_TYPE_INFO_H_
