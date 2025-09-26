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
//
// From
// https://github.com/envoyproxy/envoy/blob/master/source/common/common/random_generator.{h,cc}

#pragma once

#include <cassert>
#include <random>
#include <string>

#include "absl/strings/string_view.h"
#include "cpp2sky/internal/random_generator.h"

namespace cpp2sky {

namespace {
static constexpr size_t UUID_LENGTH = 36;
static constexpr absl::string_view CHARS =
    "0123456789AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz";
}  // namespace

class RandomGeneratorImpl : public RandomGenerator {
 public:
  std::string uuid();

 private:
  void randomBuffer(char* ch, size_t len);
};

}  // namespace cpp2sky
