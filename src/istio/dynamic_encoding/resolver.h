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

#ifndef ISTIO_DYNAMIC_ENCODING_RESOLVER_H
#define ISTIO_DYNAMIC_ENCODING_RESOLVER_H

#include <string>

#include "absl/container/flat_hash_map.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"

namespace istio {
namespace dynamic_encoding {

struct ServiceInfo {
  const ::google::protobuf::ServiceDescriptor* svc;
  std::string pkg;
  ServiceInfo(const ::google::protobuf::ServiceDescriptor* svc, std::string pkg)
      : svc(svc), pkg(pkg) {}
};

class Resolver {
 public:
  Resolver(const ::google::protobuf::FileDescriptorSet* file_descriptor_set);
  // virtual destrutor
  virtual ~Resolver() {}

  const ::google::protobuf::Descriptor* ResolveMessage(std::string name);
  const ::google::protobuf::EnumDescriptor* ResolveEnum(std::string name);

  // Resolve service contained in the proto. This is needed to construct gRPC
  // cd call.
  const ServiceInfo* ResolveService(std::string namePrefix);

 private:
  std::unique_ptr<::google::protobuf::DescriptorPool> descriptor_pool_;
  const ::google::protobuf::FileDescriptorSet* file_descriptor_set_;
  absl::flat_hash_map<std::string, const ::google::protobuf::Descriptor*>
      message_hash_map_;
  absl::flat_hash_map<std::string, const ::google::protobuf::EnumDescriptor*>
      enum_hash_map_;
  absl::flat_hash_map<std::string, std::unique_ptr<ServiceInfo>>
      service_hash_map_;
};
}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_RESOLVER_H
