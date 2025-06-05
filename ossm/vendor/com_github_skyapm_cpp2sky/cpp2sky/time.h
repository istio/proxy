// Copyright 2020 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <chrono>

#include "cpp2sky/assert.h"

namespace cpp2sky {

using SystemTime = std::chrono::system_clock::time_point;
using SteadyTime = std::chrono::steady_clock::time_point;

template <class T>
class TimePoint {
  CPP2SKY_STATIC_ASSERT(T, "Invalid time type");
};

template <>
class TimePoint<SystemTime> {
 public:
  TimePoint(SystemTime point) : point_(point) {}
  TimePoint() { point_ = std::chrono::system_clock::now(); }

  // Fetch timestamp with millisecond resolution
  int64_t fetch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               point_.time_since_epoch())
        .count();
  }

 private:
  SystemTime point_;
};

template <>
class TimePoint<SteadyTime> {
 public:
  TimePoint(SteadyTime point) : point_(point) {}
  TimePoint() { point_ = std::chrono::steady_clock::now(); }

  // Fetch timestamp with millisecond resolution
  int64_t fetch() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               point_.time_since_epoch())
        .count();
  }

 private:
  SteadyTime point_;
};

}  // namespace cpp2sky
