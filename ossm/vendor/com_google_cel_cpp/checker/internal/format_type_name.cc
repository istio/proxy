// Copyright 2025 Google LLC
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
#include "checker/internal/format_type_name.h"

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "common/type.h"
#include "common/type_kind.h"

namespace cel::checker_internal {

namespace {
struct FormatImplRecord {
  Type type;
  int offset;
};

// Parameterized types can be arbitrarily nested, so we use a vector as
// a stack to avoid overflow. Practically, we don't expect nesting
// to ever be very deep, but fuzzers and pathological inputs can easily
// trigger stack overflow with a recursive implementation.
void FormatImpl(const Type& cur, int offset,
                std::vector<FormatImplRecord>& stack, std::string* out) {
  switch (cur.kind()) {
    case TypeKind::kDyn:
      absl::StrAppend(out, "dyn");
      return;
    case TypeKind::kAny:
      absl::StrAppend(out, "any");
      return;
    case TypeKind::kBool:
      absl::StrAppend(out, "bool");
      return;
    case TypeKind::kBoolWrapper:
      absl::StrAppend(out, "wrapper(bool)");
      return;
    case TypeKind::kBytes:
      absl::StrAppend(out, "bytes");
      return;
    case TypeKind::kBytesWrapper:
      absl::StrAppend(out, "wrapper(bytes)");
      return;
    case TypeKind::kDouble:
      absl::StrAppend(out, "double");
      return;
    case TypeKind::kDoubleWrapper:
      absl::StrAppend(out, "wrapper(double)");
      return;
    case TypeKind::kDuration:
      absl::StrAppend(out, "google.protobuf.Duration");
      return;
    case TypeKind::kEnum:
      absl::StrAppend(out, "int");
      return;
    case TypeKind::kInt:
      absl::StrAppend(out, "int");
      return;
    case TypeKind::kIntWrapper:
      absl::StrAppend(out, "wrapper(int)");
      return;
    case TypeKind::kList:
      if (offset == 0) {
        absl::StrAppend(out, "list(");
        stack.push_back({cur, 1});
        stack.push_back({cur.AsList()->GetElement(), 0});
      } else {
        absl::StrAppend(out, ")");
      }
      return;
    case TypeKind::kMap:
      if (offset == 0) {
        absl::StrAppend(out, "map(");
        stack.push_back({cur, 1});
        stack.push_back({cur.AsMap()->GetKey(), 0});
        return;
      }
      if (offset == 1) {
        absl::StrAppend(out, ", ");
        stack.push_back({cur, 2});
        stack.push_back({cur.AsMap()->GetValue(), 0});
        return;
      }
      absl::StrAppend(out, ")");
      return;
    case TypeKind::kNull:
      absl::StrAppend(out, "null_type");
      return;
    case TypeKind::kOpaque: {
      OpaqueType opaque = *cur.AsOpaque();
      if (offset == 0) {
        absl::StrAppend(out, cur.AsOpaque()->name());
        if (!opaque.GetParameters().empty()) {
          absl::StrAppend(out, "(");
          stack.push_back({cur, 1});
          stack.push_back({cur.AsOpaque()->GetParameters()[0], 0});
        }
        return;
      }
      if (offset >= opaque.GetParameters().size()) {
        absl::StrAppend(out, ")");
        return;
      }
      absl::StrAppend(out, ", ");
      stack.push_back({cur, offset + 1});
      stack.push_back({cur.AsOpaque()->GetParameters()[offset], 0});
      return;
    }
    case TypeKind::kString:
      absl::StrAppend(out, "string");
      return;
    case TypeKind::kStringWrapper:
      absl::StrAppend(out, "wrapper(string)");
      return;
    case TypeKind::kStruct:
      absl::StrAppend(out, cur.AsStruct()->name());
      return;
    case TypeKind::kTimestamp:
      absl::StrAppend(out, "google.protobuf.Timestamp");
      return;
    case TypeKind::kType: {
      TypeType type_type = *cur.AsType();
      if (offset == 0) {
        absl::StrAppend(out, type_type.name());
        if (!type_type.GetParameters().empty()) {
          absl::StrAppend(out, "(");
          stack.push_back({cur, 1});
          stack.push_back({cur.AsType()->GetParameters()[0], 0});
        }
        return;
      }
      absl::StrAppend(out, ")");
      return;
    }
    case TypeKind::kTypeParam:
      absl::StrAppend(out, cur.AsTypeParam()->name());
      return;
    case TypeKind::kUint:
      absl::StrAppend(out, "uint");
      return;
    case TypeKind::kUintWrapper:
      absl::StrAppend(out, "wrapper(uint)");
      return;
    case TypeKind::kUnknown:
      absl::StrAppend(out, "*unknown*");
      return;
    case TypeKind::kError:
    case TypeKind::kFunction:
    default:
      absl::StrAppend(out, "*error*");
      return;
  }
}
}  // namespace

std::string FormatTypeName(const Type& type) {
  std::vector<FormatImplRecord> stack;
  std::string out;
  stack.push_back({type, 0});
  while (!stack.empty()) {
    auto [type, offset] = stack.back();
    stack.pop_back();
    FormatImpl(type, offset, stack, &out);
  }
  return out;
}

}  // namespace cel::checker_internal
