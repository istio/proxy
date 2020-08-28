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

#include "flatbuffers/flatbuffers.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/stubs/status.h"

/**
 * Utilities that require protobuf import.
 */
namespace Wasm {
namespace Common {

// Extract node info into a flatbuffer from a struct.
bool extractNodeFlatBuffer(const google::protobuf::Struct& metadata,
                           flatbuffers::FlatBufferBuilder& fbb);

// Extract local node metadata into a flatbuffer.
bool extractLocalNodeFlatBuffer(std::string* out);

// Extract given local node metadata into a flatbuffer.
bool extractLocalNodeFlatBuffer(std::string* out,
                                const google::protobuf::Struct& node);

// Extracts node metadata value. It looks for values of all the keys
// corresponding to EXCHANGE_KEYS in node_metadata and populates it in
// google::protobuf::Value pointer that is passed in.
google::protobuf::util::Status extractNodeMetadataValue(
    const google::protobuf::Struct& node_metadata,
    google::protobuf::Struct* metadata);

}  // namespace Common
}  // namespace Wasm
