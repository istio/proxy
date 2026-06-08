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

#include <deque>
#include <mutex>

#include "absl/types/optional.h"

namespace cpp2sky {

template <class Value>
class ValueBuffer {
 public:
  ValueBuffer() = default;

  absl::optional<Value> pop_front() {
    std::unique_lock<std::mutex> lock(mux_);
    if (buf_.empty()) {
      return absl::nullopt;
    }
    auto result = std::move(buf_.front());
    buf_.pop_front();
    return result;
  }

  /**
   * Insert new value.
   */
  void push_back(Value value) {
    std::unique_lock<std::mutex> lock(mux_);
    buf_.emplace_back(std::move(value));
  }

  /**
   * Check whether buffer is empty or not.
   */
  bool empty() const {
    std::unique_lock<std::mutex> lock(mux_);
    return buf_.empty();
  }

  /**
   * Get item count
   */
  size_t size() const {
    std::unique_lock<std::mutex> lock(mux_);
    return buf_.size();
  }

  /**
   * Clear buffer
   */
  void clear() {
    std::unique_lock<std::mutex> lock(mux_);
    buf_.clear();
  }

 private:
  std::deque<Value> buf_;
  mutable std::mutex mux_;
};

}  // namespace cpp2sky
