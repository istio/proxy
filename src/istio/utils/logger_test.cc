/* Copyright 2019 Istio Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/istio/utils/logger.h"

#include <memory>

#include "gtest/gtest.h"

namespace istio {
namespace utils {

class CountingArgument {
 public:
  const char* c_str() {
    ++to_string_calls;
    return "logged entity";
  }

  int to_string_calls{0};
};

class CountingLogger : public Logger {
 public:
  CountingLogger(int& is_loggable_calls, int& write_buffer_calls)
      : is_loggable_calls_(is_loggable_calls),
        write_buffer_calls_(write_buffer_calls) {}

  virtual bool isLoggable(Level level) override {
    ++is_loggable_calls_;

    switch (level) {
      case Level::TRACE_:
      case Level::DEBUG_:
        return false;
      case Level::INFO_:
      case Level::WARN_:
      case Level::ERROR_:
        return true;
    }
  }

  virtual void writeBuffer(Level level, const char* buffer) override {
    ++write_buffer_calls_;
  }

 private:
  int& is_loggable_calls_;
  int& write_buffer_calls_;
};

class LoggerTest : public ::testing::Test {
 protected:
  virtual void SetUp() override {
    std::unique_ptr<Logger> logger{
        new CountingLogger(is_loggable_calls_, write_buffer_calls_)};
    setLogger(std::move(logger));
    // Set logger itself logs something, so clear the counters
    is_loggable_calls_ = 0;
    write_buffer_calls_ = 0;
  }

  int is_loggable_calls_{0};
  int write_buffer_calls_{0};
};

TEST_F(LoggerTest, CallArgsOnlyIfLoggable) {
  CountingArgument entity;
  int expected_to_string_calls = 0;
  int expected_is_loggable_calls = 0;
  int expected_write_buffer_calls = 0;

  // TRACE and DEBUG shouldn't be logged and shouldn't have any affect on the
  // arguments to be logged.

  MIXER_TRACE("%s", entity.c_str());
  ++expected_is_loggable_calls;

  EXPECT_EQ(expected_to_string_calls, entity.to_string_calls);
  EXPECT_EQ(expected_is_loggable_calls, is_loggable_calls_);
  EXPECT_EQ(expected_write_buffer_calls, write_buffer_calls_);

  MIXER_DEBUG("%s", entity.c_str());
  ++expected_is_loggable_calls;

  EXPECT_EQ(expected_to_string_calls, entity.to_string_calls);
  EXPECT_EQ(expected_is_loggable_calls, is_loggable_calls_);
  EXPECT_EQ(expected_write_buffer_calls, write_buffer_calls_);

  // INFO+ will invoke their arguments once, be logged, and call isLoggable
  // twice due to a redundant/defensive isLoggable check.

  MIXER_INFO("%s", entity.c_str());
  expected_is_loggable_calls += 2;
  ++expected_to_string_calls;
  ++expected_write_buffer_calls;

  EXPECT_EQ(expected_to_string_calls, entity.to_string_calls);
  EXPECT_EQ(expected_is_loggable_calls, is_loggable_calls_);
  EXPECT_EQ(expected_write_buffer_calls, write_buffer_calls_);

  MIXER_WARN("%s", entity.c_str());
  expected_is_loggable_calls += 2;
  ++expected_to_string_calls;
  ++expected_write_buffer_calls;

  EXPECT_EQ(expected_to_string_calls, entity.to_string_calls);
  EXPECT_EQ(expected_is_loggable_calls, is_loggable_calls_);
  EXPECT_EQ(expected_write_buffer_calls, write_buffer_calls_);

  MIXER_ERROR("%s", entity.c_str());
  expected_is_loggable_calls += 2;
  ++expected_to_string_calls;
  ++expected_write_buffer_calls;

  EXPECT_EQ(expected_to_string_calls, entity.to_string_calls);
  EXPECT_EQ(expected_is_loggable_calls, is_loggable_calls_);
  EXPECT_EQ(expected_write_buffer_calls, write_buffer_calls_);
}

}  // namespace utils
}  // namespace istio
