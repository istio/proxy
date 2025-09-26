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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_LOCATION_TRACKER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_LOCATION_TRACKER_H_

#include <string>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// LocationTrackerInterface is an interface for classes that track
// the location information for the purpose of error reporting.
class LocationTrackerInterface {
 public:
  LocationTrackerInterface(const LocationTrackerInterface&) = delete;
  LocationTrackerInterface& operator=(const LocationTrackerInterface&) = delete;
  virtual ~LocationTrackerInterface() {}

  // Returns the object location as human readable string.
  virtual std::string ToString() const = 0;

 protected:
  LocationTrackerInterface() {}

 private:
  // Please do not add any data members to this class.
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_LOCATION_TRACKER_H_
