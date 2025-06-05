// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTO_EXT_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTO_EXT_H_

#include <vector>

#include "absl/status/status.h"
#include "parser/macro.h"
#include "parser/macro_registry.h"
#include "parser/options.h"

namespace cel::extensions {

// proto_macros returns the macros which are useful for working with protobuf
// objects in CEL. Specifically, the proto.getExt() and proto.hasExt() macros.
std::vector<Macro> proto_macros();

inline absl::Status RegisterProtoMacros(MacroRegistry& registry,
                                        const ParserOptions&) {
  return registry.RegisterMacros(proto_macros());
}

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTO_EXT_H_
