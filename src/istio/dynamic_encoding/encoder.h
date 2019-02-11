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

#ifndef ISTIO_DYNAMIC_ENCODING_ENCODER_H
#define ISTIO_DYNAMIC_ENCODING_ENCODER_H

#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/types/any.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/stubs/statusor.h"

namespace istio {
namespace dynamic_encoding {

// abstract Encoder Class.
class Encoder {
 public:
  Encoder() {}
  virtual ~Encoder() {}

  virtual ::google::protobuf::util::StatusOr<absl::any> Encode() = 0;

  virtual void SetAttributeBag(
      const absl::flat_hash_map<std::string, absl::any>* attribute_bag) = 0;
};
}  // namespace dynamic_encoding
}  // namespace istio

#endif  // ISTIO_DYNAMIC_ENCODING_ENCODER_H
