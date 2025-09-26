// Copyright 2024 Google LLC
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

#include <cstddef>
#include <cstring>
#include <string>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/arena.h"

namespace cel {

namespace {

struct TypeFormatter {
  void operator()(std::string* out, const Type& type) const {
    out->append(type.DebugString());
  }
};

std::string FunctionDebugString(const Type& result,
                                absl::Span<const Type> args) {
  return absl::StrCat("(", absl::StrJoin(args, ", ", TypeFormatter{}), ") -> ",
                      result.DebugString());
}

}  // namespace

namespace common_internal {

FunctionTypeData* absl_nonnull FunctionTypeData::Create(
    google::protobuf::Arena* absl_nonnull arena, const Type& result,
    absl::Span<const Type> args) {
  return ::new (arena->AllocateAligned(
      offsetof(FunctionTypeData, args) + ((1 + args.size()) * sizeof(Type)),
      alignof(FunctionTypeData))) FunctionTypeData(result, args);
}

FunctionTypeData::FunctionTypeData(const Type& result,
                                   absl::Span<const Type> args)
    : args_size(1 + args.size()) {
  this->args[0] = result;
  std::memcpy(this->args + 1, args.data(), args.size() * sizeof(Type));
}

}  // namespace common_internal

FunctionType::FunctionType(google::protobuf::Arena* absl_nonnull arena,
                           const Type& result, absl::Span<const Type> args)
    : FunctionType(
          common_internal::FunctionTypeData::Create(arena, result, args)) {}

std::string FunctionType::DebugString() const {
  return FunctionDebugString(result(), args());
}

TypeParameters FunctionType::GetParameters() const {
  ABSL_DCHECK(*this);
  return TypeParameters(absl::MakeConstSpan(data_->args, data_->args_size));
}

const Type& FunctionType::result() const {
  ABSL_DCHECK(*this);
  return data_->args[0];
}

absl::Span<const Type> FunctionType::args() const {
  ABSL_DCHECK(*this);
  return absl::MakeConstSpan(data_->args + 1, data_->args_size - 1);
}

}  // namespace cel
