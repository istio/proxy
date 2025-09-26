// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_DESCRIPTOR_TYPE_PROVIDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_DESCRIPTOR_TYPE_PROVIDER_H_

#include <memory>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/structs/legacy_type_provider.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

// Implementation of a type provider that generates types from protocol buffer
// descriptors.
class ProtobufDescriptorProvider : public LegacyTypeProvider {
 public:
  ProtobufDescriptorProvider(const google::protobuf::DescriptorPool* pool,
                             google::protobuf::MessageFactory* factory)
      : descriptor_pool_(pool), message_factory_(factory) {}

  absl::optional<LegacyTypeAdapter> ProvideLegacyType(
      absl::string_view name) const final;

  absl::optional<const LegacyTypeInfoApis*> ProvideLegacyTypeInfo(
      absl::string_view name) const final;

 private:
  // Create a new type instance if found in the registered descriptor pool.
  // Otherwise, returns nullptr.
  std::unique_ptr<ProtoMessageTypeAdapter> CreateTypeAdapter(
      absl::string_view name) const;

  const ProtoMessageTypeAdapter* GetTypeAdapter(absl::string_view name) const;

  const google::protobuf::DescriptorPool* descriptor_pool_;
  google::protobuf::MessageFactory* message_factory_;
  mutable absl::flat_hash_map<std::string,
                              std::unique_ptr<ProtoMessageTypeAdapter>>
      type_cache_ ABSL_GUARDED_BY(mu_);
  mutable absl::Mutex mu_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_DESCRIPTOR_TYPE_PROVIDER_H_
