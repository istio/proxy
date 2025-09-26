// Copyright 2016 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
//
#include "grpc_transcoding/type_helper.h"

#include <string>
#include <unordered_map>

#include "absl/strings/str_split.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/util/type_resolver.h"
#include "grpc_transcoding/percent_encoding.h"

namespace pbutil = ::google::protobuf::util;
namespace pbconv = ::google::protobuf::util::converter;

namespace google {
namespace grpc {

namespace transcoding {
namespace {

const char DEFAULT_URL_PREFIX[] = "type.googleapis.com/";

class SimpleTypeResolver : public pbutil::TypeResolver {
 public:
  SimpleTypeResolver() : url_prefix_(DEFAULT_URL_PREFIX) {}

  void AddType(const google::protobuf::Type& t) {
    type_map_.emplace(url_prefix_ + t.name(), &t);
    // A temporary workaround for service configs that use
    // "proto2.MessageOptions.*" options.
    ReplaceProto2WithGoogleProtobufInOptionNames(
        const_cast<google::protobuf::Type*>(&t));
  }

  void AddEnum(const google::protobuf::Enum& e) {
    enum_map_.emplace(url_prefix_ + e.name(), &e);
  }

  // TypeResolver implementation
  // Resolves a type url for a message type.
  virtual absl::Status ResolveMessageType(
      const std::string& type_url, google::protobuf::Type* type) override {
    auto i = type_map_.find(type_url);
    if (end(type_map_) != i) {
      if (nullptr != type) {
        *type = *i->second;
      }
      return absl::Status();
    } else {
      return absl::Status(absl::StatusCode::kNotFound,
                          "Type '" + type_url + "' cannot be found.");
    }
  }

  // Resolves a type url for an enum type.
  virtual absl::Status ResolveEnumType(
      const std::string& type_url, google::protobuf::Enum* enum_type) override {
    auto i = enum_map_.find(type_url);
    if (end(enum_map_) != i) {
      if (nullptr != enum_type) {
        *enum_type = *i->second;
      }
      return absl::Status();
    } else {
      return absl::Status(absl::StatusCode::kNotFound,
                          "Enum '" + type_url + "' cannot be found.");
    }
  }

 private:
  void ReplaceProto2WithGoogleProtobufInOptionNames(
      google::protobuf::Type* type) {
    // As a temporary workaround for service configs that use
    // "proto2.MessageOptions.*" options instead of
    // "google.protobuf.MessageOptions.*", we replace the option names to make
    // protobuf library recognize them.
    for (auto& option : *type->mutable_options()) {
      if (option.name() == "proto2.MessageOptions.map_entry") {
        option.set_name("google.protobuf.MessageOptions.map_entry");
      } else if (option.name() ==
                 "proto2.MessageOptions.message_set_wire_format") {
        option.set_name(
            "google.protobuf.MessageOptions.message_set_wire_format");
      }
    }
  }

  std::string url_prefix_;
  std::unordered_map<std::string, const google::protobuf::Type*> type_map_;
  std::unordered_map<std::string, const google::protobuf::Enum*> enum_map_;

  SimpleTypeResolver(const SimpleTypeResolver&) = delete;
  SimpleTypeResolver& operator=(const SimpleTypeResolver&) = delete;
};

class LockedTypeInfo : public pbconv::TypeInfo {
 public:
  LockedTypeInfo(pbconv::TypeInfo* type_info) : type_info_(type_info) {}

  absl::StatusOr<const google::protobuf::Type*> ResolveTypeUrl(
      absl::string_view type_url) const override {
    absl::MutexLock lock(&mutex_);
    return type_info_->ResolveTypeUrl(type_url);
  }

  const google::protobuf::Type* GetTypeByTypeUrl(
      absl::string_view type_url) const override {
    absl::MutexLock lock(&mutex_);
    return type_info_->GetTypeByTypeUrl(type_url);
  }

  const google::protobuf::Enum* GetEnumByTypeUrl(
      absl::string_view type_url) const override {
    absl::MutexLock lock(&mutex_);
    return type_info_->GetEnumByTypeUrl(type_url);
  }

  const google::protobuf::Field* FindField(
      const google::protobuf::Type* type,
      absl::string_view camel_case_name) const override {
    absl::MutexLock lock(&mutex_);
    return type_info_->FindField(type, camel_case_name);
  }

 private:
  mutable absl::Mutex mutex_;
  std::unique_ptr<pbconv::TypeInfo> type_info_ ABSL_GUARDED_BY(mutex_);
};

}  // namespace

TypeHelper::TypeHelper(pbutil::TypeResolver* type_resolver)
    : type_resolver_(type_resolver),
      type_info_(
          new LockedTypeInfo(pbconv::TypeInfo::NewTypeInfo(type_resolver))) {}

TypeHelper::~TypeHelper() {
  type_info_.reset();
  delete type_resolver_;
}

pbutil::TypeResolver* TypeHelper::Resolver() const { return type_resolver_; }

pbconv::TypeInfo* TypeHelper::Info() const { return type_info_.get(); }

void TypeHelper::Initialize() {
  type_resolver_ = new SimpleTypeResolver();
  type_info_.reset(
      new LockedTypeInfo(pbconv::TypeInfo::NewTypeInfo(type_resolver_)));
}

void TypeHelper::AddType(const google::protobuf::Type& t) {
  reinterpret_cast<SimpleTypeResolver*>(type_resolver_)->AddType(t);
}

void TypeHelper::AddEnum(const google::protobuf::Enum& e) {
  reinterpret_cast<SimpleTypeResolver*>(type_resolver_)->AddEnum(e);
}

absl::Status TypeHelper::ResolveFieldPath(
    const google::protobuf::Type& type, const std::string& field_path_str,
    std::vector<const google::protobuf::Field*>* field_path_out) const {
  // Split the field names & call ResolveFieldPath()
  const std::vector<std::string> field_names =
      absl::StrSplit(field_path_str, '.', absl::SkipEmpty());
  return ResolveFieldPath(type, field_names, field_path_out);
}

const google::protobuf::Field* TypeHelper::FindField(
    const google::protobuf::Type* type, absl::string_view name) const {
  auto* field = Info()->FindField(type, name);
  if (field != nullptr) {
    return field;
  }
  // The name may be UrlEscaped, try to un-escape it and lookup.
  absl::string_view name_view(name.data(), name.size());
  if (IsUrlEscapedString(name_view)) {
    field = Info()->FindField(type, UrlUnescapeString(name_view));
  }
  return field;
}

absl::Status TypeHelper::ResolveFieldPath(
    const google::protobuf::Type& type,
    const std::vector<std::string>& field_names,
    std::vector<const google::protobuf::Field*>* field_path_out) const {
  // The type of the current message being processed (initially the type of the
  // top level message)
  auto current_type = &type;

  // The resulting field path
  std::vector<const google::protobuf::Field*> field_path;

  for (size_t i = 0; i < field_names.size(); ++i) {
    // Find the field by name in the current type
    auto field = FindField(current_type, field_names[i]);
    if (nullptr == field) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Could not find field \"" + field_names[i] +
                              "\" in the type \"" + current_type->name() +
                              "\".");
    }
    field_path.push_back(field);

    if (i < field_names.size() - 1) {
      // If this is not the last field in the path, it must be a message
      if (google::protobuf::Field::TYPE_MESSAGE != field->kind()) {
        return absl::Status(
            absl::StatusCode::kInvalidArgument,
            "Encountered a non-leaf field \"" + field->name() +
                "\" that is not a message while parsing a field path");
      }

      // Update the type of the current message
      current_type = Info()->GetTypeByTypeUrl(field->type_url());
      if (nullptr == current_type) {
        return absl::Status(absl::StatusCode::kInvalidArgument,
                            "Cannot find the type \"" + field->type_url() +
                                "\" while parsing a field path.");
      }
    }
  }
  *field_path_out = std::move(field_path);
  return absl::Status();
}

}  // namespace transcoding

}  // namespace grpc
}  // namespace google
