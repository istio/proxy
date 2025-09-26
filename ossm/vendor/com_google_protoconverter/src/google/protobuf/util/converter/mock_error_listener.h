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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_MOCK_ERROR_LISTENER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_MOCK_ERROR_LISTENER_H_

#include <gmock/gmock.h>
#include "absl/strings/string_view.h"
#include "google/protobuf/util/converter/error_listener.h"
#include "google/protobuf/util/converter/location_tracker.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

class MockErrorListener : public ErrorListener {
 public:
  MockErrorListener() {}
  ~MockErrorListener() override {}

  MOCK_METHOD(void, InvalidName,
              (const LocationTrackerInterface& loc,
               absl::string_view unknown_name, absl::string_view message),
              (override));
  MOCK_METHOD(void, InvalidValue,
              (const LocationTrackerInterface& loc, absl::string_view type_name,
               absl::string_view value),
              (override));
  MOCK_METHOD(void, MissingField,
              (const LocationTrackerInterface& loc,
               absl::string_view missing_name),
              (override));
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_MOCK_ERROR_LISTENER_H_
