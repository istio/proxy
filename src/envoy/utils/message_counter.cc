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

namespace Envoy {
namespace Utils {

void IncrementMessageCounter(Buffer::Instance& data, GrpcMessageCounter* counter) {
  uint64_t pos = 0;
  unsigned byte = 0;
  while (pos < data.length()) {
    switch (counter->state) {
      case GrpcMessageCounter::ExpectByte0:
        // skip compress flag, increment message count
        counter->count += 1;
        counter->current_size = 0;
        pos += 1;
        counter->state = GrpcMessageCounter::ExpectByte1;
        break;
      case GrpcMessageCounter::ExpectByte1:
      case GrpcMessageCounter::ExpectByte2:
      case GrpcMessageCounter::ExpectByte3:
      case GrpcMessageCounter::ExpectByte4:
        data.copyOut(pos, 1, &byte);
        counter->current_size = counter->current_size << 8;
        counter->current_size = counter->current_size | byte;
        pos += 1;
        counter->state =
            static_cast<GrpcMessageCounter::GrpcReadState>(counter->state + 1);
        break;
      case GrpcMessageCounter::ExpectMessage:
        uint64_t available = data.length() - pos;
        if (counter->current_size <= available) {
          pos += counter->current_size;
          counter->state = GrpcMessageCounter::ExpectByte0;
        } else {
          pos = data.length();
          counter->current_size = counter->current_size - available;
        }
        break;
    }
  }
}
}  // namespace Utils
}  // namespace Envoy
