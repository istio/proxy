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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_SOURCE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_SOURCE_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"

namespace cel {

namespace common_internal {
class SourceImpl;
}  // namespace common_internal

class Source;

// SourcePosition represents an offset in source text.
using SourcePosition = int32_t;

// SourceRange represents a range of positions, where `begin` is inclusive and
// `end` is exclusive.
struct SourceRange final {
  SourcePosition begin = -1;
  SourcePosition end = -1;
};

inline bool operator==(const SourceRange& lhs, const SourceRange& rhs) {
  return lhs.begin == rhs.begin && lhs.end == rhs.end;
}

inline bool operator!=(const SourceRange& lhs, const SourceRange& rhs) {
  return !operator==(lhs, rhs);
}

// `SourceLocation` is a representation of a line and column in source text.
struct SourceLocation final {
  int32_t line = -1;    // 1-based line number.
  int32_t column = -1;  // 0-based column number.
};

inline bool operator==(const SourceLocation& lhs, const SourceLocation& rhs) {
  return lhs.line == rhs.line && lhs.column == rhs.column;
}

inline bool operator!=(const SourceLocation& lhs, const SourceLocation& rhs) {
  return !operator==(lhs, rhs);
}

// `SourceContentView` is a view of the content owned by `Source`, which is a
// sequence of Unicode code points.
class SourceContentView final {
 public:
  SourceContentView(const SourceContentView&) = default;
  SourceContentView(SourceContentView&&) = default;
  SourceContentView& operator=(const SourceContentView&) = default;
  SourceContentView& operator=(SourceContentView&&) = default;

  SourcePosition size() const;

  bool empty() const;

  char32_t at(SourcePosition position) const;

  std::string ToString(SourcePosition begin, SourcePosition end) const;
  std::string ToString(SourcePosition begin) const {
    return ToString(begin, size());
  }
  std::string ToString() const { return ToString(0); }

  void AppendToString(std::string& dest) const;

 private:
  friend class Source;

  constexpr SourceContentView() = default;

  constexpr explicit SourceContentView(absl::Span<const char> view)
      : view_(view) {}

  constexpr explicit SourceContentView(absl::Span<const uint8_t> view)
      : view_(view) {}

  constexpr explicit SourceContentView(absl::Span<const char16_t> view)
      : view_(view) {}

  constexpr explicit SourceContentView(absl::Span<const char32_t> view)
      : view_(view) {}

  absl::variant<absl::Span<const char>, absl::Span<const uint8_t>,
                absl::Span<const char16_t>, absl::Span<const char32_t>>
      view_;
};

// `Source` represents the source expression.
class Source {
 public:
  using ContentView = SourceContentView;

  Source(const Source&) = delete;
  Source(Source&&) = delete;

  virtual ~Source() = default;

  Source& operator=(const Source&) = delete;
  Source& operator=(Source&&) = delete;

  virtual absl::string_view description() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  // Maps a `SourcePosition` to a `SourceLocation`. Returns an empty
  // `absl::optional` when `SourcePosition` is invalid or the information
  // required to perform the mapping is not present.
  absl::optional<SourceLocation> GetLocation(SourcePosition position) const;

  // Maps a `SourceLocation` to a `SourcePosition`. Returns an empty
  // `absl::optional` when `SourceLocation` is invalid or the information
  // required to perform the mapping is not present.
  absl::optional<SourcePosition> GetPosition(
      const SourceLocation& location) const;

  absl::optional<std::string> Snippet(int32_t line) const;

  // Formats an annotated snippet highlighting an error at location, e.g.
  //
  // "\n | $SOURCE_SNIPPET" +
  // "\n | .......^"
  //
  // Returns an empty string if location is not a valid location in this source.
  std::string DisplayErrorLocation(SourceLocation location) const;

  // Returns a view of the underlying expression text, if present.
  virtual ContentView content() const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  // Returns a `absl::Span` of `SourcePosition` which represent the positions
  // where new lines occur.
  virtual absl::Span<const SourcePosition> line_offsets() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

 protected:
  static constexpr ContentView EmptyContentView() { return ContentView(); }
  static constexpr ContentView MakeContentView(absl::Span<const char> view) {
    return ContentView(view);
  }
  static constexpr ContentView MakeContentView(absl::Span<const uint8_t> view) {
    return ContentView(view);
  }
  static constexpr ContentView MakeContentView(
      absl::Span<const char16_t> view) {
    return ContentView(view);
  }
  static constexpr ContentView MakeContentView(
      absl::Span<const char32_t> view) {
    return ContentView(view);
  }

 private:
  friend class common_internal::SourceImpl;

  Source() = default;

  absl::optional<SourcePosition> FindLinePosition(int32_t line) const;

  absl::optional<std::pair<int32_t, SourcePosition>> FindLine(
      SourcePosition position) const;
};

using SourcePtr = std::unique_ptr<Source>;

absl::StatusOr<absl_nonnull SourcePtr> NewSource(
    absl::string_view content, std::string description = "<input>");

absl::StatusOr<absl_nonnull SourcePtr> NewSource(
    const absl::Cord& content, std::string description = "<input>");

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_SOURCE_H_
