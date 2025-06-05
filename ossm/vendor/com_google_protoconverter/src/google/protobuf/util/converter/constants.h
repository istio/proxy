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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_CONSTANTS_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_CONSTANTS_H_

#include <cstdint>

// This file contains constants used by //net/proto2/util/converter.

namespace google {
namespace protobuf {
namespace util {
namespace converter {
// Prefix for type URLs.
const char kTypeServiceBaseUrl[] = "type.googleapis.com";

// Format string for RFC3339 timestamp formatting.
const char kRfc3339TimeFormat[] = "%E4Y-%m-%dT%H:%M:%S";

// Same as above, but the year value is not zero-padded i.e. this accepts
// timestamps like "1-01-0001T23:59:59Z" instead of "0001-01-0001T23:59:59Z".
const char kRfc3339TimeFormatNoPadding[] = "%Y-%m-%dT%H:%M:%S";

// Minimum seconds allowed in a google.protobuf.Timestamp value.
const int64_t kTimestampMinSeconds = -62135596800LL;

// Maximum seconds allowed in a google.protobuf.Timestamp value.
const int64_t kTimestampMaxSeconds = 253402300799LL;

// Minimum seconds allowed in a google.protobuf.Duration value.
const int64_t kDurationMinSeconds = -315576000000LL;

// Maximum seconds allowed in a google.protobuf.Duration value.
const int64_t kDurationMaxSeconds = 315576000000LL;

// Nano seconds in a second.
const int32_t kNanosPerSecond = 1000000000;

// Type url representing NULL values in google.protobuf.Struct type.
const char kStructNullValueTypeUrl[] =
    "type.googleapis.com/google.protobuf.NullValue";

// Type string for google.protobuf.Struct
const char kStructType[] = "google.protobuf.Struct";

// Type string for struct.proto's google.protobuf.Value value type.
const char kStructValueType[] = "google.protobuf.Value";

// Type string for struct.proto's google.protobuf.ListValue value type.
const char kStructListValueType[] = "google.protobuf.ListValue";

// Type string for google.protobuf.Timestamp
const char kTimestampType[] = "google.protobuf.Timestamp";

// Type string for google.protobuf.Duration
const char kDurationType[] = "google.protobuf.Duration";

// Type URL for struct value type google.protobuf.Value
const char kStructValueTypeUrl[] = "type.googleapis.com/google.protobuf.Value";

// Type string for google.protobuf.Any
const char kAnyType[] = "google.protobuf.Any";

// The protobuf option name of jspb.message_id;
const char kOptionJspbMessageId[] = "jspb.message_id";

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_CONSTANTS_H_
