// Copyright 2021 SkyAPM

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <chrono>

#include "cpp2sky/time.h"

namespace cpp2sky {
class Timer {
 public:
  Timer(int64_t interval_sec) {
    interval_ = interval_sec * 1000;
    prev_time_ = TimePoint<SteadyTime>().fetch();
  }

  bool check() {
    const auto current_time = TimePoint<SteadyTime>().fetch();
    bool result = current_time - prev_time_ > interval_;
    if (result) {
      prev_time_ = current_time;
    }

    return result;
  }

 private:
  int64_t prev_time_;
  int64_t interval_;
};
}  // namespace cpp2sky
