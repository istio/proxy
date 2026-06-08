// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_REGISTER_FUNCTION_HELPER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_REGISTER_FUNCTION_HELPER_H_

#include <utility>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/function_descriptor.h"
#include "runtime/function_registry.h"
namespace cel {

// Helper class for performing registration with function adapter.
//
// Usage:
//
// auto status = RegisterHelper<BinaryFunctionAdapter<bool, int64_t, int64_t>>
//   ::RegisterGlobalOverload(
//     '_<_',
//     [](int64_t x, int64_t y) -> bool {return x < y},
//     registry);
//
// if (!status.ok) return status;
//
// Note: if using this with status macros (*RETURN_IF_ERROR), an extra set of
// parentheses is needed around the multi-argument template specifier.
template <typename AdapterT>
class RegisterHelper {
 public:
  // Generic registration for an adapted function. Prefer using one of the more
  // specific Register* functions.
  template <typename FunctionT>
  static absl::Status Register(absl::string_view name, bool receiver_style,
                               FunctionT&& fn, FunctionRegistry& registry,
                               bool strict) {
    return registry.Register(
        AdapterT::CreateDescriptor(name, receiver_style, strict),
        AdapterT::WrapFunction(std::forward<FunctionT>(fn)));
  }

  template <typename FunctionT>
  static absl::Status Register(absl::string_view name, bool receiver_style,
                               FunctionT&& fn, FunctionRegistry& registry,
                               FunctionDescriptorOptions options = {}) {
    return registry.Register(
        AdapterT::CreateDescriptor(name, receiver_style, options),
        AdapterT::WrapFunction(std::forward<FunctionT>(fn)));
  }

  // Registers a global overload (.e.g. size(<list>) )
  template <typename FunctionT>
  static absl::Status RegisterGlobalOverload(absl::string_view name,
                                             FunctionT&& fn,
                                             FunctionRegistry& registry) {
    return Register(name, /*receiver_style=*/false, std::forward<FunctionT>(fn),
                    registry);
  }

  // Registers a member overload (.e.g. <list>.size())
  template <typename FunctionT>
  static absl::Status RegisterMemberOverload(absl::string_view name,
                                             FunctionT&& fn,
                                             FunctionRegistry& registry) {
    return Register(name, /*receiver_style=*/true, std::forward<FunctionT>(fn),
                    registry);
  }

  // Registers a non-strict overload.
  //
  // Non-strict functions may receive errors or unknown values as arguments,
  // and must correctly propagate them.
  //
  // Most extension functions should prefer 'strict' overloads where the
  // evaluator handles unknown and error propagation.
  template <typename FunctionT>
  static absl::Status RegisterNonStrictOverload(absl::string_view name,
                                                FunctionT&& fn,
                                                FunctionRegistry& registry) {
    return Register(name, /*receiver_style=*/false, std::forward<FunctionT>(fn),
                    registry, /*strict=*/false);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_REGISTER_FUNCTION_HELPER_H_
