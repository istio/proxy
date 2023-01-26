/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#pragma once

#include "extensions/common/node_info_generated.h"
#include "flatbuffers/flatbuffers.h"
#include "google/protobuf/struct.pb.h"

/**
 * Utilities that require protobuf import.
 */
namespace Wasm {
namespace Common {

// Extract node info into a flatbuffer from a struct.
flatbuffers::DetachedBuffer
extractNodeFlatBufferFromStruct(const google::protobuf::Struct& metadata);

// Extract struct from a flatbuffer. This is an inverse of the above function.
void extractStructFromNodeFlatBuffer(const FlatNode& node, google::protobuf::Struct* metadata);

// Serialize deterministically a protobuf to a string.
bool serializeToStringDeterministic(const google::protobuf::Message& metadata,
                                    std::string* metadata_bytes);

} // namespace Common
} // namespace Wasm
