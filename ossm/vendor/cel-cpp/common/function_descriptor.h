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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_FUNCTION_DESCRIPTOR_H_
#define THIRD_PARTY_CEL_CPP_COMMON_FUNCTION_DESCRIPTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/kind.h"

namespace cel {

struct FunctionDescriptorOptions {
  // If true (strict, default), error or unknown arguments are propagated
  // instead of calling the function. if false (non-strict), the function may
  // receive error or unknown values as arguments.
  bool is_strict = true;

  // Whether the function is impure or context-sensitive.
  //
  // Impure functions depend on state other than the arguments received during
  // the CEL expression evaluation or have visible side effects. This breaks
  // some of the assumptions of the CEL evaluation model. This flag is used as a
  // hint to the planner that some optimizations are not safe or not effective.
  bool is_contextual = false;
};

// Coarsely describes a function for the purpose of runtime resolution of
// overloads.
class FunctionDescriptor final {
 public:
  FunctionDescriptor(absl::string_view name, bool receiver_style,
                     std::vector<Kind> types, bool is_strict)
      : impl_(std::make_shared<Impl>(
            name, std::move(types), receiver_style,
            FunctionDescriptorOptions{is_strict,
                                      /*is_contextual=*/false})) {}

  FunctionDescriptor(absl::string_view name, bool receiver_style,
                     std::vector<Kind> types, bool is_strict,
                     bool is_contextual)
      : impl_(std::make_shared<Impl>(
            name, std::move(types), receiver_style,
            FunctionDescriptorOptions{is_strict, is_contextual})) {}

  FunctionDescriptor(absl::string_view name, bool is_receiver_style,
                     std::vector<Kind> types,
                     FunctionDescriptorOptions options = {})
      : impl_(std::make_shared<Impl>(name, std::move(types), is_receiver_style,
                                     options)) {}

  // Function name.
  const std::string& name() const { return impl_->name; }

  // Whether function is receiver style i.e. true means arg0.name(args[1:]...).
  bool receiver_style() const { return impl_->is_receiver_style; }

  // The argument types the function accepts.
  //
  // TODO(uncreated-issue/17): make this kinds
  const std::vector<Kind>& types() const { return impl_->types; }

  // if true (strict, default), error or unknown arguments are propagated
  // instead of calling the function. if false (non-strict), the function may
  // receive error or unknown values as arguments.
  bool is_strict() const { return impl_->options.is_strict; }

  // Whether the function is contextual (impure).
  //
  // Contextual functions depend on state other than the arguments received in
  // the CEL expression evaluation or have visible side effects. This breaks
  // some of the assumptions of CEL. This flag is used as a hint to the planner
  // that some optimizations are not safe or not effective.
  bool is_contextual() const { return impl_->options.is_contextual; }

  // Helper for matching a descriptor. This tests that the shape is the same --
  // |other| accepts the same number and types of arguments and is the same call
  // style).
  bool ShapeMatches(const FunctionDescriptor& other) const {
    return ShapeMatches(other.receiver_style(), other.types());
  }
  bool ShapeMatches(bool receiver_style, absl::Span<const Kind> types) const;

  bool operator==(const FunctionDescriptor& other) const;

  bool operator<(const FunctionDescriptor& other) const;

 private:
  struct Impl final {
    Impl(absl::string_view name, std::vector<Kind> types,
         bool is_receiver_style, FunctionDescriptorOptions options)
        : name(name),
          types(std::move(types)),
          is_receiver_style(is_receiver_style),
          options(options) {}

    std::string name;
    std::vector<Kind> types;
    bool is_receiver_style;
    FunctionDescriptorOptions options;
  };

  std::shared_ptr<const Impl> impl_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_FUNCTION_DESCRIPTOR_H_
