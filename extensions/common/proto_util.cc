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

#include "extensions/common/proto_util.h"

#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

namespace Wasm {
namespace Common {

bool serializeToStringDeterministic(const google::protobuf::Message& metadata,
                                    std::string* metadata_bytes) {
  google::protobuf::io::StringOutputStream md(metadata_bytes);
  google::protobuf::io::CodedOutputStream mcs(&md);

  mcs.SetSerializationDeterministic(true);
  if (!metadata.SerializeToCodedStream(&mcs)) {
    return false;
  }
  return true;
}

} // namespace Common
} // namespace Wasm
