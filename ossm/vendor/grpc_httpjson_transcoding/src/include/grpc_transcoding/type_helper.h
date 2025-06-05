/* Copyright 2016 Google Inc. All Rights Reserved.
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
#ifndef GRPC_TRANSCODING_TYPE_HELPER_H_
#define GRPC_TRANSCODING_TYPE_HELPER_H_

#include <memory>
#include <vector>

#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/util/type_resolver.h"

namespace google {
namespace grpc {

namespace transcoding {

// Provides ::google::protobuf::util::TypeResolver and
// ::google::protobuf::util::converter::TypeInfo implementations based on a
// collection of types and a collection of enums.
// This object is thread-safe
class TypeHelper {
 public:
  template <typename Types, typename Enums>
  TypeHelper(const Types& types, const Enums& enums);

  TypeHelper(::google::protobuf::util::TypeResolver* type_resolver);

  ~TypeHelper();

  ::google::protobuf::util::TypeResolver* Resolver() const;
  ::google::protobuf::util::converter::TypeInfo* Info() const;

  // Takes a string representation of a field path & resolves it into actual
  // protobuf Field pointers.
  //
  // A field path is a sequence of fields that identifies a potentially nested
  // field in the message. It can be empty as well, which identifies the entire
  // message.
  // E.g. "shelf.theme" field path would correspond to the "theme" field of the
  // "shelf" field of the top-level message. The type of the top-level message
  // is passed to ResolveFieldPath().
  //
  // The string representation of the field path is just the dot-delimited
  // list of the field names or empty:
  //    FieldPath = "" | Field {"." Field};
  //    Field     = <protobuf field name>;
  absl::Status ResolveFieldPath(
      const ::google::protobuf::Type& type, const std::string& field_path_str,
      std::vector<const ::google::protobuf::Field*>* field_path) const;

  // Resolve a field path specified through a vector of field names into a
  // vector of actual protobuf Field pointers.
  // Similiar to the above method but accepts the field path as a vector of
  // names instead of one dot-delimited string.
  absl::Status ResolveFieldPath(
      const ::google::protobuf::Type& type,
      const std::vector<std::string>& field_path_unresolved,
      std::vector<const ::google::protobuf::Field*>* field_path_resolved) const;

 private:
  void Initialize();
  void AddType(const ::google::protobuf::Type& t);
  void AddEnum(const ::google::protobuf::Enum& e);

  const google::protobuf::Field* FindField(const google::protobuf::Type* type,
                                           absl::string_view name) const;

  ::google::protobuf::util::TypeResolver* type_resolver_;
  std::unique_ptr<::google::protobuf::util::converter::TypeInfo> type_info_;

  TypeHelper() = delete;
  TypeHelper(const TypeHelper&) = delete;
  TypeHelper& operator=(const TypeHelper&) = delete;
};

template <typename Types, typename Enums>
TypeHelper::TypeHelper(const Types& types, const Enums& enums)
    : type_resolver_(nullptr), type_info_() {
  Initialize();
  for (const auto& t : types) {
    AddType(t);
  }
  for (const auto& e : enums) {
    AddEnum(e);
  }
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google

#endif  // GRPC_TRANSCODING_TYPE_HELPER_H_
