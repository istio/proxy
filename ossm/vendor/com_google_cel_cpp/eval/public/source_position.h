/*
 * Copyright 2018 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SOURCE_POSITION_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SOURCE_POSITION_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Class representing the source position as well as line and column data for
// a given expression id.
class SourcePosition {
 public:
  // Constructor for a SourcePosition value. The source_info may be nullptr,
  // in which case line, column, and character_offset will return 0.
  SourcePosition(const int64_t expr_id,
                 const google::api::expr::v1alpha1::SourceInfo* source_info)
      : expr_id_(expr_id), source_info_(source_info) {}

  // Non-copyable
  SourcePosition(const SourcePosition& other) = delete;
  SourcePosition& operator=(const SourcePosition& other) = delete;

  virtual ~SourcePosition() {}

  // Return the 1-based source line number for the expression.
  int32_t line() const;

  // Return the 1-based column offset within the source line for the
  // expression.
  int32_t column() const;

  // Return the 0-based character offset of the expression within source.
  int32_t character_offset() const;

 private:
  // The expression identifier.
  const int64_t expr_id_;
  // The source information reference generated during expression parsing.
  const google::api::expr::v1alpha1::SourceInfo* source_info_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_SOURCE_POSITION_H_
