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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_LOCATION_TRACKER_H_
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_LOCATION_TRACKER_H_

#include <string>

#include "google/protobuf/util/converter/location_tracker.h"

namespace google {
namespace protobuf {
namespace util {
namespace converter {

// An empty concrete implementation of LocationTrackerInterface.
class ObjectLocationTracker : public LocationTrackerInterface {
 public:
  // Creates an empty location tracker.
  ObjectLocationTracker() {}
  ObjectLocationTracker(const ObjectLocationTracker&) = delete;
  ObjectLocationTracker& operator=(const ObjectLocationTracker&) = delete;

  ~ObjectLocationTracker() override {}

  // Returns empty because nothing is tracked.
  std::string ToString() const override { return ""; }
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_OBJECT_LOCATION_TRACKER_H_
