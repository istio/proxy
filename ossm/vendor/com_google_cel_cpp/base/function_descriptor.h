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

#ifndef THIRD_PARTY_CEL_CPP_BASE_FUNCTION_DESCRIPTOR_H_
#define THIRD_PARTY_CEL_CPP_BASE_FUNCTION_DESCRIPTOR_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/kind.h"

namespace cel {

// Describes a function.
class FunctionDescriptor final {
 public:
  FunctionDescriptor(absl::string_view name, bool receiver_style,
                     std::vector<Kind> types, bool is_strict = true)
      : impl_(std::make_shared<Impl>(name, receiver_style, std::move(types),
                                     is_strict)) {}

  // Function name.
  const std::string& name() const { return impl_->name; }

  // Whether function is receiver style i.e. true means arg0.name(args[1:]...).
  bool receiver_style() const { return impl_->receiver_style; }

  // The argmument types the function accepts.
  //
  // TODO: make this kinds
  const std::vector<Kind>& types() const { return impl_->types; }

  // if true (strict, default), error or unknown arguments are propagated
  // instead of calling the function. if false (non-strict), the function may
  // receive error or unknown values as arguments.
  bool is_strict() const { return impl_->is_strict; }

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
    Impl(absl::string_view name, bool receiver_style, std::vector<Kind> types,
         bool is_strict)
        : name(name),
          types(std::move(types)),
          receiver_style(receiver_style),
          is_strict(is_strict) {}

    std::string name;
    std::vector<Kind> types;
    bool receiver_style;
    bool is_strict;
  };

  std::shared_ptr<const Impl> impl_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_FUNCTION_DESCRIPTOR_H_
