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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cpp2sky/internal/async_client.h"
#include "cpp2sky/internal/random_generator.h"

using testing::_;
using testing::Return;

namespace cpp2sky {

class MockRandomGenerator : public RandomGenerator {
 public:
  MockRandomGenerator() { ON_CALL(*this, uuid).WillByDefault(Return("uuid")); }
  MOCK_METHOD(std::string, uuid, ());
};

class MockTraceAsyncStream : public TraceAsyncStream {
 public:
  MOCK_METHOD(void, sendMessage, (TraceRequestType));
};

class MockTraceAsyncClient : public TraceAsyncClient {
 public:
  MOCK_METHOD(void, sendMessage, (TraceRequestType));
  MOCK_METHOD(void, resetClient, ());
};

}  // namespace cpp2sky
