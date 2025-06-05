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

#include "common/source.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/inlined_vector.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "internal/unicode.h"
#include "internal/utf8.h"

namespace cel {

SourcePosition SourceContentView::size() const {
  return static_cast<SourcePosition>(absl::visit(
      absl::Overload(
          [](absl::Span<const char> view) { return view.size(); },
          [](absl::Span<const uint8_t> view) { return view.size(); },
          [](absl::Span<const char16_t> view) { return view.size(); },
          [](absl::Span<const char32_t> view) { return view.size(); }),
      view_));
}

bool SourceContentView::empty() const {
  return absl::visit(
      absl::Overload(
          [](absl::Span<const char> view) { return view.empty(); },
          [](absl::Span<const uint8_t> view) { return view.empty(); },
          [](absl::Span<const char16_t> view) { return view.empty(); },
          [](absl::Span<const char32_t> view) { return view.empty(); }),
      view_);
}

char32_t SourceContentView::at(SourcePosition position) const {
  ABSL_DCHECK_GE(position, 0);
  ABSL_DCHECK_LT(position, size());
  return absl::visit(
      absl::Overload(
          [position =
               static_cast<size_t>(position)](absl::Span<const char> view) {
            return static_cast<char32_t>(static_cast<uint8_t>(view[position]));
          },
          [position =
               static_cast<size_t>(position)](absl::Span<const uint8_t> view) {
            return static_cast<char32_t>(view[position]);
          },
          [position =
               static_cast<size_t>(position)](absl::Span<const char16_t> view) {
            return static_cast<char32_t>(view[position]);
          },
          [position =
               static_cast<size_t>(position)](absl::Span<const char32_t> view) {
            return static_cast<char32_t>(view[position]);
          }),
      view_);
}

std::string SourceContentView::ToString(SourcePosition begin,
                                        SourcePosition end) const {
  ABSL_DCHECK_GE(begin, 0);
  ABSL_DCHECK_LE(end, size());
  ABSL_DCHECK_LE(begin, end);
  return absl::visit(
      absl::Overload(
          [begin = static_cast<size_t>(begin),
           end = static_cast<size_t>(end)](absl::Span<const char> view) {
            view = view.subspan(begin, end - begin);
            return std::string(view.data(), view.size());
          },
          [begin = static_cast<size_t>(begin),
           end = static_cast<size_t>(end)](absl::Span<const uint8_t> view) {
            view = view.subspan(begin, end - begin);
            std::string result;
            result.reserve(view.size() * 2);
            for (const auto& code_point : view) {
              internal::Utf8Encode(result, code_point);
            }
            result.shrink_to_fit();
            return result;
          },
          [begin = static_cast<size_t>(begin),
           end = static_cast<size_t>(end)](absl::Span<const char16_t> view) {
            view = view.subspan(begin, end - begin);
            std::string result;
            result.reserve(view.size() * 3);
            for (const auto& code_point : view) {
              internal::Utf8Encode(result, code_point);
            }
            result.shrink_to_fit();
            return result;
          },
          [begin = static_cast<size_t>(begin),
           end = static_cast<size_t>(end)](absl::Span<const char32_t> view) {
            view = view.subspan(begin, end - begin);
            std::string result;
            result.reserve(view.size() * 4);
            for (const auto& code_point : view) {
              internal::Utf8Encode(result, code_point);
            }
            result.shrink_to_fit();
            return result;
          }),
      view_);
}

void SourceContentView::AppendToString(std::string& dest) const {
  absl::visit(absl::Overload(
                  [&dest](absl::Span<const char> view) {
                    dest.append(view.data(), view.size());
                  },
                  [&dest](absl::Span<const uint8_t> view) {
                    for (const auto& code_point : view) {
                      internal::Utf8Encode(dest, code_point);
                    }
                  },
                  [&dest](absl::Span<const char16_t> view) {
                    for (const auto& code_point : view) {
                      internal::Utf8Encode(dest, code_point);
                    }
                  },
                  [&dest](absl::Span<const char32_t> view) {
                    for (const auto& code_point : view) {
                      internal::Utf8Encode(dest, code_point);
                    }
                  }),
              view_);
}

namespace common_internal {

class SourceImpl : public Source {
 public:
  SourceImpl(std::string description,
             absl::InlinedVector<SourcePosition, 1> line_offsets)
      : description_(std::move(description)),
        line_offsets_(std::move(line_offsets)) {}

  absl::string_view description() const final { return description_; }

  absl::Span<const SourcePosition> line_offsets() const final {
    return absl::MakeConstSpan(line_offsets_);
  }

 private:
  const std::string description_;
  const absl::InlinedVector<SourcePosition, 1> line_offsets_;
};

namespace {

class AsciiSource final : public SourceImpl {
 public:
  AsciiSource(std::string description,
              absl::InlinedVector<SourcePosition, 1> line_offsets,
              std::vector<char> text)
      : SourceImpl(std::move(description), std::move(line_offsets)),
        text_(std::move(text)) {}

  ContentView content() const override {
    return MakeContentView(absl::MakeConstSpan(text_));
  }

 private:
  const std::vector<char> text_;
};

class Latin1Source final : public SourceImpl {
 public:
  Latin1Source(std::string description,
               absl::InlinedVector<SourcePosition, 1> line_offsets,
               std::vector<uint8_t> text)
      : SourceImpl(std::move(description), std::move(line_offsets)),
        text_(std::move(text)) {}

  ContentView content() const override {
    return MakeContentView(absl::MakeConstSpan(text_));
  }

 private:
  const std::vector<uint8_t> text_;
};

class BasicPlaneSource final : public SourceImpl {
 public:
  BasicPlaneSource(std::string description,
                   absl::InlinedVector<SourcePosition, 1> line_offsets,
                   std::vector<char16_t> text)
      : SourceImpl(std::move(description), std::move(line_offsets)),
        text_(std::move(text)) {}

  ContentView content() const override {
    return MakeContentView(absl::MakeConstSpan(text_));
  }

 private:
  const std::vector<char16_t> text_;
};

class SupplementalPlaneSource final : public SourceImpl {
 public:
  SupplementalPlaneSource(std::string description,
                          absl::InlinedVector<SourcePosition, 1> line_offsets,
                          std::vector<char32_t> text)
      : SourceImpl(std::move(description), std::move(line_offsets)),
        text_(std::move(text)) {}

  ContentView content() const override {
    return MakeContentView(absl::MakeConstSpan(text_));
  }

 private:
  const std::vector<char32_t> text_;
};

template <typename T>
struct SourceTextTraits;

template <>
struct SourceTextTraits<absl::string_view> {
  using iterator_type = absl::string_view;

  static iterator_type Begin(absl::string_view text) { return text; }

  static void Advance(iterator_type& it, size_t n) { it.remove_prefix(n); }

  static void AppendTo(std::vector<uint8_t>& out, absl::string_view text,
                       size_t n) {
    const auto* in = reinterpret_cast<const uint8_t*>(text.data());
    out.insert(out.end(), in, in + n);
  }

  static std::vector<char> ToVector(absl::string_view in) {
    std::vector<char> out;
    out.reserve(in.size());
    out.insert(out.end(), in.begin(), in.end());
    return out;
  }
};

template <>
struct SourceTextTraits<absl::Cord> {
  using iterator_type = absl::Cord::CharIterator;

  static iterator_type Begin(const absl::Cord& text) {
    return text.char_begin();
  }

  static void Advance(iterator_type& it, size_t n) {
    absl::Cord::Advance(&it, n);
  }

  static void AppendTo(std::vector<uint8_t>& out, const absl::Cord& text,
                       size_t n) {
    auto it = text.char_begin();
    while (n > 0) {
      auto str = absl::Cord::ChunkRemaining(it);
      size_t to_append = std::min(n, str.size());
      const auto* in = reinterpret_cast<const uint8_t*>(str.data());
      out.insert(out.end(), in, in + to_append);
      n -= to_append;
      absl::Cord::Advance(&it, to_append);
    }
  }

  static std::vector<char> ToVector(const absl::Cord& in) {
    std::vector<char> out;
    out.reserve(in.size());
    for (const auto& chunk : in.Chunks()) {
      out.insert(out.end(), chunk.begin(), chunk.end());
    }
    return out;
  }
};

template <typename T>
absl::StatusOr<SourcePtr> NewSourceImpl(std::string description, const T& text,
                                        const size_t text_size) {
  if (ABSL_PREDICT_FALSE(
          text_size >
          static_cast<size_t>(std::numeric_limits<int32_t>::max()))) {
    return absl::InvalidArgumentError("expression larger than 2GiB limit");
  }
  using Traits = SourceTextTraits<T>;
  size_t index = 0;
  typename Traits::iterator_type it = Traits::Begin(text);
  SourcePosition offset = 0;
  char32_t code_point;
  size_t code_units;
  std::vector<uint8_t> data8;
  std::vector<char16_t> data16;
  std::vector<char32_t> data32;
  absl::InlinedVector<SourcePosition, 1> line_offsets;
  while (index < text_size) {
    std::tie(code_point, code_units) = cel::internal::Utf8Decode(it);
    if (ABSL_PREDICT_FALSE(code_point ==
                               cel::internal::kUnicodeReplacementCharacter &&
                           code_units == 1)) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("cannot parse malformed UTF-8 input");
    }
    if (code_point == '\n') {
      line_offsets.push_back(offset + 1);
    }
    if (code_point <= 0x7f) {
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      continue;
    }
    if (code_point <= 0xff) {
      data8.reserve(text_size);
      Traits::AppendTo(data8, text, index);
      data8.push_back(static_cast<uint8_t>(code_point));
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      goto latin1;
    }
    if (code_point <= 0xffff) {
      data16.reserve(text_size);
      for (size_t offset = 0; offset < index; offset++) {
        data16.push_back(static_cast<uint8_t>(text[offset]));
      }
      data16.push_back(static_cast<char16_t>(code_point));
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      goto basic;
    }
    data32.reserve(text_size);
    for (size_t offset = 0; offset < index; offset++) {
      data32.push_back(static_cast<char32_t>(text[offset]));
    }
    data32.push_back(code_point);
    Traits::Advance(it, code_units);
    index += code_units;
    ++offset;
    goto supplemental;
  }
  line_offsets.push_back(offset + 1);
  return std::make_unique<AsciiSource>(
      std::move(description), std::move(line_offsets), Traits::ToVector(text));
latin1:
  while (index < text_size) {
    std::tie(code_point, code_units) = internal::Utf8Decode(it);
    if (ABSL_PREDICT_FALSE(code_point ==
                               internal::kUnicodeReplacementCharacter &&
                           code_units == 1)) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("cannot parse malformed UTF-8 input");
    }
    if (code_point == '\n') {
      line_offsets.push_back(offset + 1);
    }
    if (code_point <= 0xff) {
      data8.push_back(static_cast<uint8_t>(code_point));
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      continue;
    }
    if (code_point <= 0xffff) {
      data16.reserve(text_size);
      for (const auto& value : data8) {
        data16.push_back(value);
      }
      std::vector<uint8_t>().swap(data8);
      data16.push_back(static_cast<char16_t>(code_point));
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      goto basic;
    }
    data32.reserve(text_size);
    for (const auto& value : data8) {
      data32.push_back(value);
    }
    std::vector<uint8_t>().swap(data8);
    data32.push_back(code_point);
    Traits::Advance(it, code_units);
    index += code_units;
    ++offset;
    goto supplemental;
  }
  line_offsets.push_back(offset + 1);
  return std::make_unique<Latin1Source>(
      std::move(description), std::move(line_offsets), std::move(data8));
basic:
  while (index < text_size) {
    std::tie(code_point, code_units) = internal::Utf8Decode(it);
    if (ABSL_PREDICT_FALSE(code_point ==
                               internal::kUnicodeReplacementCharacter &&
                           code_units == 1)) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("cannot parse malformed UTF-8 input");
    }
    if (code_point == '\n') {
      line_offsets.push_back(offset + 1);
    }
    if (code_point <= 0xffff) {
      data16.push_back(static_cast<char16_t>(code_point));
      Traits::Advance(it, code_units);
      index += code_units;
      ++offset;
      continue;
    }
    data32.reserve(text_size);
    for (const auto& value : data16) {
      data32.push_back(static_cast<char32_t>(value));
    }
    std::vector<char16_t>().swap(data16);
    data32.push_back(code_point);
    Traits::Advance(it, code_units);
    index += code_units;
    ++offset;
    goto supplemental;
  }
  line_offsets.push_back(offset + 1);
  return std::make_unique<BasicPlaneSource>(
      std::move(description), std::move(line_offsets), std::move(data16));
supplemental:
  while (index < text_size) {
    std::tie(code_point, code_units) = internal::Utf8Decode(it);
    if (ABSL_PREDICT_FALSE(code_point ==
                               internal::kUnicodeReplacementCharacter &&
                           code_units == 1)) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("cannot parse malformed UTF-8 input");
    }
    if (code_point == '\n') {
      line_offsets.push_back(offset + 1);
    }
    data32.push_back(code_point);
    Traits::Advance(it, code_units);
    index += code_units;
    ++offset;
  }
  line_offsets.push_back(offset + 1);
  return std::make_unique<SupplementalPlaneSource>(
      std::move(description), std::move(line_offsets), std::move(data32));
}

}  // namespace

}  // namespace common_internal

absl::optional<SourceLocation> Source::GetLocation(
    SourcePosition position) const {
  if (auto line_and_offset = FindLine(position);
      ABSL_PREDICT_TRUE(line_and_offset.has_value())) {
    return SourceLocation{line_and_offset->first,
                          position - line_and_offset->second};
  }
  return absl::nullopt;
}

absl::optional<SourcePosition> Source::GetPosition(
    const SourceLocation& location) const {
  if (ABSL_PREDICT_FALSE(location.line < 1 || location.column < 0)) {
    return absl::nullopt;
  }
  if (auto position = FindLinePosition(location.line);
      ABSL_PREDICT_TRUE(position.has_value())) {
    return *position + location.column;
  }
  return absl::nullopt;
}

absl::optional<std::string> Source::Snippet(int32_t line) const {
  auto content = this->content();
  auto start = FindLinePosition(line);
  if (ABSL_PREDICT_FALSE(!start.has_value() || content.empty())) {
    return absl::nullopt;
  }
  auto end = FindLinePosition(line + 1);
  if (end.has_value()) {
    return content.ToString(*start, *end - 1);
  }
  return content.ToString(*start);
}

std::string Source::DisplayErrorLocation(SourceLocation location) const {
  constexpr char32_t kDot = '.';
  constexpr char32_t kHat = '^';

  constexpr char32_t kWideDot = 0xff0e;
  constexpr char32_t kWideHat = 0xff3e;
  absl::optional<std::string> snippet = Snippet(location.line);
  if (!snippet || snippet->empty()) {
    return "";
  }

  *snippet = absl::StrReplaceAll(*snippet, {{"\t", " "}});
  absl::string_view snippet_view(*snippet);
  std::string result;
  absl::StrAppend(&result, "\n | ", *snippet);
  absl::StrAppend(&result, "\n | ");

  std::string index_line;
  for (int32_t i = 0; i < location.column && !snippet_view.empty(); ++i) {
    size_t count;
    std::tie(std::ignore, count) = internal::Utf8Decode(snippet_view);
    snippet_view.remove_prefix(count);
    if (count > 1) {
      internal::Utf8Encode(index_line, kWideDot);
    } else {
      internal::Utf8Encode(index_line, kDot);
    }
  }
  size_t count = 0;
  if (!snippet_view.empty()) {
    std::tie(std::ignore, count) = internal::Utf8Decode(snippet_view);
  }
  if (count > 1) {
    internal::Utf8Encode(index_line, kWideHat);
  } else {
    internal::Utf8Encode(index_line, kHat);
  }
  absl::StrAppend(&result, index_line);
  return result;
}

absl::optional<SourcePosition> Source::FindLinePosition(int32_t line) const {
  if (ABSL_PREDICT_FALSE(line < 1)) {
    return absl::nullopt;
  }
  if (line == 1) {
    return SourcePosition{0};
  }
  const auto line_offsets = this->line_offsets();
  if (ABSL_PREDICT_TRUE(line <= static_cast<int32_t>(line_offsets.size()))) {
    return line_offsets[static_cast<size_t>(line - 2)];
  }
  return absl::nullopt;
}

absl::optional<std::pair<int32_t, SourcePosition>> Source::FindLine(
    SourcePosition position) const {
  if (ABSL_PREDICT_FALSE(position < 0)) {
    return absl::nullopt;
  }
  int32_t line = 1;
  const auto line_offsets = this->line_offsets();
  for (const auto& line_offset : line_offsets) {
    if (line_offset > position) {
      break;
    }
    ++line;
  }
  if (line == 1) {
    return std::make_pair(line, SourcePosition{0});
  }
  return std::make_pair(line, line_offsets[static_cast<size_t>(line) - 2]);
}

absl::StatusOr<absl::Nonnull<SourcePtr>> NewSource(absl::string_view content,
                                                   std::string description) {
  return common_internal::NewSourceImpl(std::move(description), content,
                                        content.size());
}

absl::StatusOr<absl::Nonnull<SourcePtr>> NewSource(const absl::Cord& content,
                                                   std::string description) {
  return common_internal::NewSourceImpl(std::move(description), content,
                                        content.size());
}

}  // namespace cel
