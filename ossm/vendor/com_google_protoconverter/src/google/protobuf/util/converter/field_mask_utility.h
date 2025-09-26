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

// FieldMask related utility methods.

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_FIELD_MASK_UTILITY_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_FIELD_MASK_UTILITY_H_

#include <functional>
#include <stack>

#include "google/protobuf/stubs/callback.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"


namespace google {
namespace protobuf {
namespace util {
namespace converter {

typedef std::function<std::string(absl::string_view)> ConverterCallback;
typedef std::function<absl::Status(absl::string_view)> PathSinkCallback;

// Applies a 'converter' to each segment of a FieldMask path and returns the
// result. Quoted strings in the 'path' are copied to the output as-is without
// converting their content. Escaping is supported within quoted strings.
// For example, "ab\"_c" will be returned as "ab\"_c" without any changes.
std::string ConvertFieldMaskPath(const absl::string_view path,
                                 ConverterCallback converter);

// Decodes a compact list of FieldMasks. For example, "a.b,a.c.d,a.c.e" will be
// decoded into a list of field paths - "a.b", "a.c.d", "a.c.e". And the results
// will be sent to 'path_sink', i.e. 'path_sink' will be called once per
// resulting path.
// Note that we also support Apiary style FieldMask form. The above example in
// the Apiary style will look like "a.b,a.c(d,e)".
absl::Status DecodeCompactFieldMaskPaths(absl::string_view paths,
                                         PathSinkCallback path_sink);

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_FIELD_MASK_UTILITY_H_
