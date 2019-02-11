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
#include "resolver.h"
#include <exception>
#include <memory>

#include "absl/memory/memory.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"

namespace istio {
namespace dynamic_encoding {
namespace {}

Resolver::Resolver(
    const ::google::protobuf::FileDescriptorSet* file_descriptor_set)
    : descriptor_pool_(absl::make_unique<::google::protobuf::DescriptorPool>()),
      file_descriptor_set_(file_descriptor_set) {
  for (auto& file_descriptor_proto : file_descriptor_set->file()) {
    const google::protobuf::FileDescriptor* file_descriptor =
        descriptor_pool_->BuildFile(file_descriptor_proto);
    if (file_descriptor != nullptr) {
      for (int index = 0; index < file_descriptor->message_type_count();
           index++) {
        const google::protobuf::Descriptor* message_descriptor =
            file_descriptor->message_type(index);
        message_hash_map_[message_descriptor->full_name()] = message_descriptor;
      }

      for (int index = 0; index < file_descriptor->enum_type_count(); index++) {
        const google::protobuf::EnumDescriptor* enum_descriptor =
            file_descriptor->enum_type(index);
        enum_hash_map_[enum_descriptor->full_name()] = enum_descriptor;
      }

      for (int index = 0; index < file_descriptor->service_count(); index++) {
        const google::protobuf::ServiceDescriptor* service_descriptor =
            file_descriptor->service(index);
        std::unique_ptr<ServiceInfo> service_info(
            new ServiceInfo(service_descriptor, file_descriptor->package()));
        service_hash_map_[service_descriptor->full_name()] =
            std::move(service_info);
      }
    }
  }
}

const ::google::protobuf::Descriptor* Resolver::ResolveMessage(
    std::string name) {
  auto result = message_hash_map_.find(name);
  if (result != message_hash_map_.end()) {
    return result->second;
  }

  return nullptr;
}

const ::google::protobuf::EnumDescriptor* Resolver::ResolveEnum(
    std::string name) {
  return enum_hash_map_[name];
}

// Resolve service contained in the proto
const ServiceInfo* Resolver::ResolveService(std::string namePrefix) {
  return service_hash_map_[namePrefix].get();
}

}  // namespace dynamic_encoding
}  // namespace istio
