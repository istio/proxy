/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include "absl/strings/string_view.h"
#include "src/envoy/http/authn_wasm/cert.h"

#ifndef NULL_PLUGIN
#include "proxy_wasm_intrinsics.h"
#else
#include "include/proxy-wasm/null_plugin.h"

using proxy_wasm::null_plugin::getProperty;
using proxy_wasm::null_plugin::logDebug;

namespace proxy_wasm {
namespace null_plugin {
namespace Http {
namespace AuthN {

#endif

constexpr absl::string_view Connection = "connection";
constexpr absl::string_view TlsVersion = "tls_version";
constexpr absl::string_view UriSanPeerCertificate = "uri_san_peer_certificate";
constexpr absl::string_view LocalSanPeerCertificate =
    "uri_san_local_certificate";
constexpr absl::string_view Mtls = "mtls";

class ConnectionContext {
 public:
  ConnectionContext() = default;

  bool isMtls() const { return mtls_; }

  // Regard this connection as TLS when we can extract tls version.
  bool isTls() const {
    return getProperty({Connection, TlsVersion}).has_value();
  }

  const TlsPeerCertificateInfoPtr& peerCertificateInfo() const {
    return peer_cert_info_;
  }

  const TlsLocalCertificateInfoPtr& localCertificateInfo() const {
    return local_cert_info_;
  }

 private:
  TlsPeerCertificateInfoPtr peer_cert_info_;
  TlsLocalCertificateInfoPtr local_cert_info_;

  bool mtls_;
};

#ifdef NULL_PLUGIN

}  // namespace AuthN
}  // namespace Http
}  // namespace null_plugin
}  // namespace proxy_wasm

#endif