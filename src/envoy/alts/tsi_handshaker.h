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

class TsiHandshakerCallbacks {
 public:
  virtual ~TsiHandshakerCallbacks() {}

  struct NextResult {
    tsi_result status_;
    Buffer::InstancePtr to_send_;
    tsi_handshaker_result* result_;
  };

  virtual void onNextDone(NextResult&& result) PURE;
};

class TsiHandshaker {
 public:
  explicit TsiHandshaker(tsi_handshaker* handshaker);
  virtual ~TsiHandshaker();

  tsi_result Next(Buffer::Instance& received);
  void setHandshakerCallbacks(TsiHandshakerCallbacks& callbacks) {
    callbacks_ = &callbacks;
  }

 private:
  tsi_handshaker* handshaker_{};
  TsiHandshakerCallbacks* callbacks_{};
};

typedef std::unique_ptr<TsiHandshaker> TsiHandshakerPtr;
}
}