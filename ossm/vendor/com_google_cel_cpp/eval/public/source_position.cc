// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/source_position.h"

#include <utility>

namespace google {
namespace api {
namespace expr {
namespace runtime {

using cel::expr::SourceInfo;

namespace {

std::pair<int, int32_t> GetLineAndLineOffset(const SourceInfo* source_info,
                                           int32_t position) {
  int line = 0;
  int32_t line_offset = 0;
  if (source_info != nullptr) {
    for (const auto& curr_line_offset : source_info->line_offsets()) {
      if (curr_line_offset > position) {
        break;
      }
      line_offset = curr_line_offset;
      line++;
    }
  }
  if (line == 0) {
    line++;
  }
  return std::pair<int, int32_t>(line, line_offset);
}

}  // namespace

int32_t SourcePosition::line() const {
  return GetLineAndLineOffset(source_info_, character_offset()).first;
}

int32_t SourcePosition::column() const {
  int32_t position = character_offset();
  std::pair<int, int32_t> line_and_offset =
      GetLineAndLineOffset(source_info_, position);
  return 1 + (position - line_and_offset.second);
}

int32_t SourcePosition::character_offset() const {
  if (source_info_ == nullptr) {
    return 0;
  }
  auto position_it = source_info_->positions().find(expr_id_);
  return position_it != source_info_->positions().end() ? position_it->second
                                                        : 0;
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
