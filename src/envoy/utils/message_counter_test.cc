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

#include "src/envoy/utils/message_counter.h"

#include "common/buffer/buffer_impl.h"

#include "test/common/buffer/utility.h"
#include "test/test_common/utility.h"

namespace Envoy {
namespace Utils {

namespace {

TEST(MessageCounterTest, IncrementMessageCounter) {
  {
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte0);
    EXPECT_EQ(counter.count, 0);
  }

  {
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    Buffer::addSeq(buffer, {0});
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte1);
    EXPECT_EQ(counter.count, 1);
  }

  {
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    Buffer::addSeq(buffer, {1, 0, 0, 0, 1, 0xFF});
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte0);
    EXPECT_EQ(counter.count, 1);
  }

  {
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    Buffer::addSeq(buffer, {1, 0, 0, 0, 1, 0xFF});
    Buffer::addSeq(buffer, {0, 0, 0, 0, 2, 0xFF, 0xFF});
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte0);
    EXPECT_EQ(counter.count, 2);
  }

  {
    Buffer::OwnedImpl buffer1;
    Buffer::OwnedImpl buffer2;
    GrpcMessageCounter counter;
    // message spans two buffers
    Buffer::addSeq(buffer1, {1, 0, 0, 0, 2, 0xFF});
    Buffer::addSeq(buffer2, {0xFF, 0, 0, 0, 0, 2, 0xFF, 0xFF});
    IncrementMessageCounter(buffer1, &counter);
    IncrementMessageCounter(buffer2, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte0);
    EXPECT_EQ(counter.count, 2);
  }

  {
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    // Add longer byte sequence
    Buffer::addSeq(buffer, {1, 0, 0, 1, 0});
    Buffer::addRepeated(buffer, 1 << 8, 0xFF);
    // Start second message
    Buffer::addSeq(buffer, {0});
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.state, GrpcMessageCounter::GrpcReadState::ExpectByte1);
    EXPECT_EQ(counter.count, 2);
  }

  {
    // two empty messages
    Buffer::OwnedImpl buffer;
    GrpcMessageCounter counter;
    Buffer::addRepeated(buffer, 10, 0);
    IncrementMessageCounter(buffer, &counter);
    EXPECT_EQ(counter.count, 2);
  }
}

}  // namespace

}  // namespace Utils
}  // namespace Envoy
