/* Copyright 2018 Istio Authors. All Rights Reserved.
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
#include <mutex>

#include "envoy/buffer/buffer.h"
#include "envoy/event/dispatcher.h"

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

  typedef std::unique_ptr<NextResult> NextResultPtr;

  virtual void onNextDone(NextResultPtr&& result) PURE;
};

class TsiHandshaker : Event::DeferredDeletable {
 public:
  explicit TsiHandshaker(tsi_handshaker* handshaker,
                         Event::Dispatcher& dispatcher);
  virtual ~TsiHandshaker();

  tsi_result next(Buffer::Instance& received);
  void setHandshakerCallbacks(TsiHandshakerCallbacks& callbacks) {
    callbacks_ = &callbacks;
  }
  void deferredDelete();

 private:
  static void onNextDone(tsi_result status, void* user_data,
                         const unsigned char* bytes_to_send,
                         size_t bytes_to_send_size,
                         tsi_handshaker_result* handshaker_result);

  tsi_handshaker* handshaker_{nullptr};
  TsiHandshakerCallbacks* callbacks_{nullptr};
  bool calling_{false};
  bool delete_on_done_{false};
  Event::Dispatcher& dispatcher_;
};

typedef std::unique_ptr<TsiHandshaker> TsiHandshakerPtr;
}
}
