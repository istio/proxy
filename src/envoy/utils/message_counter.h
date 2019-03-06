/* Copyright 2017 Istio Authors. All Rights Reserved.
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

#pragma once

#include "envoy/buffer/buffer.h"

namespace Envoy {
namespace Utils {

// The struct to store gRPC message counter state.
struct GrpcMessageCounter {
  GrpcMessageCounter() : state(ExpectByte0), current_size(0), count(0){};

  // gRPC uses 5 byte header to encode subsequent message length
  enum GrpcReadState {
    ExpectByte0 = 0,
    ExpectByte1,
    ExpectByte2,
    ExpectByte3,
    ExpectByte4,
    ExpectMessage
  };

  // current read state
  GrpcReadState state;

  // current message size
  uint64_t current_size;

  // message counter
  uint64_t count;
};

// Detect gRPC message boundaries and increment the counters: each message is
// prefixed by 5 bytes length-prefix
// https://github.com/grpc/grpc/blob/master/doc/PROTOCOL-HTTP2.md
void IncrementMessageCounter(Buffer::Instance& data,
                             GrpcMessageCounter* counter);

}  // namespace Utils
}  // namespace Envoy
