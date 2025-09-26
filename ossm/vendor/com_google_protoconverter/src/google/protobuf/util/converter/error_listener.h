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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_ERROR_LISTENER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_ERROR_LISTENER_H_

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/stubs/callback.h"
#include "google/protobuf/util/converter/location_tracker.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// Interface for error listener.
class ErrorListener {
 public:
  ErrorListener(const ErrorListener&) = delete;
  ErrorListener& operator=(const ErrorListener&) = delete;
  virtual ~ErrorListener() {}

  // Reports an invalid name at the given location.
  virtual void InvalidName(const LocationTrackerInterface& loc,
                           absl::string_view invalid_name,
                           absl::string_view message) = 0;

  // Reports an invalid value for a field.
  virtual void InvalidValue(const LocationTrackerInterface& loc,
                            absl::string_view type_name,
                            absl::string_view value) = 0;

  // Reports a missing required field.
  virtual void MissingField(const LocationTrackerInterface& loc,
                            absl::string_view missing_name) = 0;

 protected:
  ErrorListener() {}
};

// An error listener that ignores all errors.
class NoopErrorListener : public ErrorListener {
 public:
  NoopErrorListener() {}
  NoopErrorListener(const NoopErrorListener&) = delete;
  NoopErrorListener& operator=(const NoopErrorListener&) = delete;
  ~NoopErrorListener() override {}

  void InvalidName(const LocationTrackerInterface& /*loc*/,
                   absl::string_view /* invalid_name */,
                   absl::string_view /* message */) override {}

  void InvalidValue(const LocationTrackerInterface& /*loc*/,
                    absl::string_view /* type_name */,
                    absl::string_view /* value */) override {}

  void MissingField(const LocationTrackerInterface& /* loc */,
                    absl::string_view /* missing_name */) override {}
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_ERROR_LISTENER_H_
