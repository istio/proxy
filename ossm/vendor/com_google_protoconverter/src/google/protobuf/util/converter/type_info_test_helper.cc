// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "google/protobuf/util/converter/type_info_test_helper.h"

#include <memory>
#include <vector>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "google/protobuf/util/converter/constants.h"
#include "google/protobuf/util/converter/default_value_objectwriter.h"
#include "google/protobuf/util/converter/protostream_objectsource.h"
#include "google/protobuf/util/converter/protostream_objectwriter.h"
#include "google/protobuf/util/converter/type_info.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/util/type_resolver.h"
#include "google/protobuf/util/type_resolver_util.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {
namespace testing {


void TypeInfoTestHelper::ResetTypeInfo(
    const std::vector<const Descriptor*>& descriptors) {
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      const DescriptorPool* pool = descriptors[0]->file()->pool();
      for (int i = 1; i < descriptors.size(); ++i) {
        ABSL_QCHECK(pool == descriptors[i]->file()->pool())
            << "Descriptors from different pools are not supported.";
      }
      type_resolver_.reset(
          NewTypeResolverForDescriptorPool(kTypeServiceBaseUrl, pool));
      typeinfo_.reset(TypeInfo::NewTypeInfo(type_resolver_.get()));
      return;
    }
  }
  ABSL_LOG(FATAL) << "Can not reach here.";
}

void TypeInfoTestHelper::ResetTypeInfo(const Descriptor* descriptor) {
  std::vector<const Descriptor*> descriptors;
  descriptors.push_back(descriptor);
  ResetTypeInfo(descriptors);
}

void TypeInfoTestHelper::ResetTypeInfo(const Descriptor* descriptor1,
                                       const Descriptor* descriptor2) {
  std::vector<const Descriptor*> descriptors;
  descriptors.push_back(descriptor1);
  descriptors.push_back(descriptor2);
  ResetTypeInfo(descriptors);
}

TypeInfo* TypeInfoTestHelper::GetTypeInfo() { return typeinfo_.get(); }

ProtoStreamObjectSource* TypeInfoTestHelper::NewProtoSource(
    io::CodedInputStream* coded_input, const std::string& type_url,
    ProtoStreamObjectSource::RenderOptions render_options) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new ProtoStreamObjectSource(coded_input, type_resolver_.get(),
                                         *type, render_options);
    }
  }
  ABSL_LOG(FATAL) << "Can not reach here.";
  return nullptr;
}

ProtoStreamObjectWriter* TypeInfoTestHelper::NewProtoWriter(
    const std::string& type_url, strings::ByteSink* output,
    ErrorListener* listener, const ProtoStreamObjectWriter::Options& options) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new ProtoStreamObjectWriter(type_resolver_.get(), *type, output,
                                         listener, options);
    }
  }
  ABSL_LOG(FATAL) << "Can not reach here.";
  return nullptr;
}

DefaultValueObjectWriter* TypeInfoTestHelper::NewDefaultValueWriter(
    const std::string& type_url, ObjectWriter* writer) {
  const google::protobuf::Type* type = typeinfo_->GetTypeByTypeUrl(type_url);
  switch (type_) {
    case USE_TYPE_RESOLVER: {
      return new DefaultValueObjectWriter(type_resolver_.get(), *type, writer);
    }
  }
  ABSL_LOG(FATAL) << "Can not reach here.";
  return nullptr;
}

}  // namespace testing
}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
