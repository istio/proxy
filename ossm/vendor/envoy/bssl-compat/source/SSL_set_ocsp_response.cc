/*
 * Copyright (C) 2022 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <openssl/ssl.h>
#include <ossl.h>
#include "SSL_CTX_set_select_certificate_cb.h"


typedef std::pair<void*,size_t> OcspResponse;

static int index() {
  static int index {ossl.ossl_SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr)};
  return index;
}

/**
 * This callback gets installed via SSL_CTX_set_tlsext_status_cb(...) in order to deal
 * with the deferred OCSP response that may have been set via SSL_set_ocsp_response()
 */
static int ssl_apply_deferred_ocsp_response_cb(SSL *ssl, void *arg) {
  std::unique_ptr<OcspResponse> resp {reinterpret_cast<OcspResponse*>(ossl.ossl_SSL_get_ex_data(ssl, index()))};

  if (resp) {
    ossl.ossl_SSL_set_ex_data(ssl, index(), nullptr);
    if (ossl.ossl_SSL_set_tlsext_status_ocsp_resp(ssl, resp->first, resp->second) == 0) {
      return ossl_SSL_TLSEXT_ERR_ALERT_FATAL;
    }
    return ossl_SSL_TLSEXT_ERR_OK;
  }

  return ossl_SSL_TLSEXT_ERR_NOACK;
}

/**
 * If this is called from within the select certificate callback, then we don't call
 * ossl_SSL_CTX_set_tlsext_status_cb() directly because it doesn't work from within that
 * callback. Instead, we squirel away the OCSP response bytes to be applied later on via
 * ossl_SSL_CTX_set_tlsext_status_cb() later on.
 */
extern "C" int SSL_set_ocsp_response(SSL *ssl, const uint8_t *response, size_t response_len) {
  if (void *response_copy {ossl.ossl_OPENSSL_memdup(response, response_len)}) {
    if (in_select_certificate_cb(ssl)) {

      SSL_CTX *ctx {ossl.ossl_SSL_get_SSL_CTX(ssl)};
      int (*callback)(SSL *, void *) {nullptr};

      // Check that we are not overwriting another existing callback
      if (ossl_SSL_CTX_get_tlsext_status_cb(ctx, &callback) == 0) {
        return 0;
      }
      if (callback && (callback != ssl_apply_deferred_ocsp_response_cb)) {
        return 0;
      }

      // Install our callback to call the real SSL_set_ex_data() function later
      if (ossl_SSL_CTX_set_tlsext_status_cb(ctx, ssl_apply_deferred_ocsp_response_cb) == 0) {
        return 0;
      }

      // Store the OCSP response bytes for the callback to pick up later
      return ossl.ossl_SSL_set_ex_data(ssl, index(), new OcspResponse(response_copy, response_len));
    }
    else {
      return ossl.ossl_SSL_set_tlsext_status_ocsp_resp(ssl, response_copy, response_len);
    }
  }

  return response ? 0 : 1;
}
