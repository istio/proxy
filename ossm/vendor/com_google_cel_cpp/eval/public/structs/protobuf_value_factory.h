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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_VALUE_FACTORY_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_VALUE_FACTORY_H_

#include <functional>

#include "google/protobuf/message.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime::internal {

// Definiton for factory producing a properly initialized message-typed
// CelValue.
//
// google::protobuf::Message is assumed adapted as possible, so this function just
// associates it with appropriate type information.
//
// Used to break cyclic dependency between field access and message wrapping --
// not intended for general use.
using ProtobufValueFactory = CelValue (*)(const google::protobuf::Message*);
}  // namespace google::api::expr::runtime::internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_PROTOBUF_VALUE_FACTORY_H_
