/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSPB_CONFIG_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSPB_CONFIG_H_

#include "absl/log/absl_check.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// List of JSPB variants used for translation when JSPB format is requested.
enum class JspbFormat {
  UNKNOWN,
  FAVA,
  GWT,
  LITE,
};

struct FavaConfig {
  // See internal-link-go/jspb-wire-format-details#PIVOT_LIMIT
  static constexpr int kPivotLimit = 1024;
  static constexpr int kDefaultPivot = 500;

  FavaConfig& set_use_nonstandard_json(bool use_nonstandard_json) {
    this->use_nonstandard_json = use_nonstandard_json;
    return *this;
  }

  FavaConfig& set_use_boolean_true_false(bool use_boolean_true_false) {
    this->use_boolean_true_false = use_boolean_true_false;
    return *this;
  }

  FavaConfig& set_use_jspb_message_id(bool use_jspb_message_id) {
    this->use_jspb_message_id = use_jspb_message_id;
    return *this;
  }

  FavaConfig& set_accept_negative_unsigned_number(
      bool accept_negative_unsigned_number) {
    this->accept_negative_unsigned_number = accept_negative_unsigned_number;
    return *this;
  }

  FavaConfig& set_pivot(int pivot) {
    ABSL_CHECK_LT(pivot, kPivotLimit);
    ABSL_CHECK_GE(pivot, 0);
    this->pivot = pivot;
    return *this;
  }

  // Whether generates compact, non-standard JSON, e.g.
  // "[1,null,null,{'1234':'foo'}]" -> "[1,,,{1234:'foo'}]".
  bool use_nonstandard_json = false;

  // Whether to write boolean values as "true"/"false" instead of "1"/"0"
  // respectively. This option does not affect parsing, where both
  // "true"/"false" and "1"/"0" are accepted.
  bool use_boolean_true_false = false;

  // Whether generates jspb.message_id for response messages. A 1-based index
  // data array is used for response messages with jspb.message_id option, e.g.
  // For message with option (jspb.message_id) = "msg":
  // "[1,null,null]" -> "["msg", 1,null,null]".
  // This is a temporary option for rollout purpose.
  bool use_jspb_message_id = false;

  // Whether to accept signed values for unsigned int proto fields.
  bool accept_negative_unsigned_number = false;

  // Fields with field numbers above or equal to the pivot will be serialized
  // into the an object (as the last element of the result array) to avoid
  // large array indices.
  int pivot = kDefaultPivot;
};

struct JspbConfig {
  JspbConfig& set_format(JspbFormat format) {
    this->format = format;
    return *this;
  }

  JspbConfig& set_fava_config(const FavaConfig& fava_config) {
    this->fava = fava_config;
    return *this;
  }

  JspbConfig& set_ignore_unknown_fields(bool ignore_unknown_fields) {
    this->ignore_unknown_fields = ignore_unknown_fields;
    return *this;
  }

  JspbConfig& set_use_lower_camel_for_enums(bool use_lower_camel_for_enums) {
    this->use_lower_camel_for_enums = use_lower_camel_for_enums;
    return *this;
  }

  // Whether to ignore unknown fields when translating to protobuf.
  bool ignore_unknown_fields = false;

  bool use_lower_camel_for_enums = false;

  // JSPB format
  JspbFormat format = JspbFormat::UNKNOWN;

  // FAVA JSPB specific config
  FavaConfig fava;
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSPB_CONFIG_H_
