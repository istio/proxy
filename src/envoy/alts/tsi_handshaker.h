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

#include <memory>

#include "envoy/buffer/buffer.h"

#include "src/core/tsi/transport_security_interface.h"

namespace Envoy {
namespace Security {

class TsiHandshaker {
 public:
  explicit TsiHandshaker(tsi_handshaker* handshaker);
  virtual ~TsiHandshaker();

  typedef std::function<void(
      tsi_result status, const unsigned char* bytes_to_send,
      size_t bytes_to_send_size, tsi_handshaker_result* handshaker_result)>
      OnNextDoneCb;

  tsi_result Next(Buffer::Instance& received, OnNextDoneCb cb);

 private:
  tsi_handshaker* handshaker_;
};

typedef std::unique_ptr<TsiHandshaker> TsiHandshakerPtr;
}
}