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
#include "src/envoy/alts/tsi_handshaker.h"

namespace Envoy {
namespace Security {

namespace {

void TsiHandshakerOnNextDone(tsi_result status, void *user_data,
                             const unsigned char *bytes_to_send,
                             size_t bytes_to_send_size,
                             tsi_handshaker_result *handshaker_result) {
  TsiHandshaker::OnNextDoneCb *raw_cb =
      reinterpret_cast<TsiHandshaker::OnNextDoneCb *>(user_data);
  if (*raw_cb) {
    (*raw_cb)(status, bytes_to_send, bytes_to_send_size, handshaker_result);
  }
  delete raw_cb;
}

}  // namespace

TsiHandshaker::TsiHandshaker(tsi_handshaker *handshaker)
    : handshaker_(handshaker) {}

TsiHandshaker::~TsiHandshaker() {
  tsi_handshaker_destroy(handshaker_);
  handshaker_ = nullptr;
}

tsi_result TsiHandshaker::Next(Envoy::Buffer::Instance &received,
                               TsiHandshaker::OnNextDoneCb cb) {
  uint64_t received_size = received.length();

  const unsigned char *bytes_to_send = nullptr;
  size_t bytes_to_send_size = 0;
  tsi_handshaker_result *result = nullptr;
  OnNextDoneCb *cb_pointer = new OnNextDoneCb();
  cb_pointer->swap(cb);
  tsi_result status =
      tsi_handshaker_next(handshaker_, reinterpret_cast<const unsigned char *>(
                                           received.linearize(received_size)),
                          received_size, &bytes_to_send, &bytes_to_send_size,
                          &result, TsiHandshakerOnNextDone, cb_pointer);

  if (status != TSI_ASYNC) {
    TsiHandshakerOnNextDone(status, cb_pointer, bytes_to_send,
                            bytes_to_send_size, result);
  }
  return status;
}
}
}
