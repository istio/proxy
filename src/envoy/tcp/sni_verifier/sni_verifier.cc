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

#include "src/envoy/tcp/sni_verifier/sni_verifier.h"

#include "envoy/buffer/buffer.h"
#include "envoy/common/exception.h"
#include "envoy/network/connection.h"
#include "envoy/stats/scope.h"

#include "common/common/assert.h"

#include "openssl/bytestring.h"
#include "openssl/ssl.h"

namespace Envoy {
namespace Tcp {
namespace SniVerifier {

Config::Config(Stats::Scope& scope, uint32_t max_client_hello_size)
    : stats_{SNI_VERIFIER_STATS(POOL_COUNTER_PREFIX(scope, "sni_verifier."))},
      ssl_ctx_(SSL_CTX_new(TLS_with_buffers_method())),
      max_client_hello_size_(max_client_hello_size) {

  if (max_client_hello_size_ > TLS_MAX_CLIENT_HELLO) {
    throw EnvoyException(fmt::format("max_client_hello_size of {} is greater than maximum of {}.",
                                     max_client_hello_size_, size_t(TLS_MAX_CLIENT_HELLO)));
  }

  SSL_CTX_set_options(ssl_ctx_.get(), SSL_OP_NO_TICKET);
  SSL_CTX_set_session_cache_mode(ssl_ctx_.get(), SSL_SESS_CACHE_OFF);
  SSL_CTX_set_tlsext_servername_callback(
      ssl_ctx_.get(), [](SSL* ssl, int* out_alert, void*) -> int {
        SniVerifierFilter* filter = static_cast<SniVerifierFilter*>(SSL_get_app_data(ssl));
        filter->onServername(SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name));

        // Return an error to stop the handshake; we have what we wanted already.
        *out_alert = SSL_AD_USER_CANCELLED;
        return SSL_TLSEXT_ERR_ALERT_FATAL;
      });
}

bssl::UniquePtr<SSL> Config::newSsl() { return bssl::UniquePtr<SSL>{SSL_new(ssl_ctx_.get())}; }

thread_local uint8_t SniVerifierFilter::buf_[Config::TLS_MAX_CLIENT_HELLO];

SniVerifierFilter::SniVerifierFilter(const ConfigSharedPtr config)
    : config_(config), ssl_(config_->newSsl()) {
  RELEASE_ASSERT(sizeof(buf_) >= config_->maxClientHelloSize(), "");

  SSL_set_app_data(ssl_.get(), this);
  SSL_set_accept_state(ssl_.get());
}

Network::FilterStatus SniVerifierFilter::onData(Buffer::Instance& data, bool) {
  ENVOY_CONN_LOG(trace, "SniVerifier: got {} bytes", read_callbacks_->connection(),
                 data.length());
  if (done_) {
    return is_match_ ? Network::FilterStatus::Continue : Network::FilterStatus::StopIteration;
  }

  size_t freeSpaceInBuf = sizeof(buf_) - read_;
  size_t lenToRead = (data.length() < freeSpaceInBuf) ? data.length() : freeSpaceInBuf;
  uint8_t* bufToParse = buf_ + read_;
  data.copyOut(0, lenToRead, bufToParse);
  read_ += lenToRead;
  parseClientHello(bufToParse, lenToRead);

  return is_match_ ? Network::FilterStatus::Continue : Network::FilterStatus::StopIteration;
}

void SniVerifierFilter::onServername(absl::string_view servername) {
  if (!servername.empty()) {
    config_->stats().sni_found_.inc();
    absl::string_view outerSni = read_callbacks_->connection().requestedServerName();
    if (servername == outerSni) {
        is_match_ = true;
    }
    ENVOY_LOG(debug, "sni_verifier:onServerName(), requestedServerName: {}", servername);
  } else {
    config_->stats().sni_not_found_.inc();
  }
  clienthello_success_ = true;
}

void SniVerifierFilter::done(bool success) {
  ENVOY_LOG(trace, "sni_verifier: done: {}", success);
  done_ = true;
  if (success) {
    read_callbacks_->continueReading();
  }
}

void SniVerifierFilter::parseClientHello(const void* data, size_t len) {
  // Ownership is passed to ssl_ in SSL_set_bio()
  bssl::UniquePtr<BIO> bio(BIO_new_mem_buf(data, len));

  // Make the mem-BIO return that there is more data
  // available beyond it's end
  BIO_set_mem_eof_return(bio.get(), -1);

  SSL_set_bio(ssl_.get(), bio.get(), bio.get());
  bio.release();

  int ret = SSL_do_handshake(ssl_.get());

  // This should never succeed because an error is always returned from the SNI callback.
  ASSERT(ret <= 0);
  switch (SSL_get_error(ssl_.get(), ret)) {
  case SSL_ERROR_WANT_READ:
    if (read_ == config_->maxClientHelloSize()) {
      // We've hit the specified size limit. This is an unreasonably large ClientHello;
      // indicate failure.
      config_->stats().client_hello_too_large_.inc();
      done(false);
    }
    break;
  case SSL_ERROR_SSL:
    if (clienthello_success_) {
      config_->stats().tls_found_.inc();
    } else {
      config_->stats().tls_not_found_.inc();
    }
    done(true);
    break;
  default:
    done(false);
    break;
  }
}

} // namespace SniVerifier
} // namespace Tcp
} // namespace Envoy
