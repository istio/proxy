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

#include "config.h"

#include "common/filesystem/filesystem_impl.h"
#include "common/json/json_loader.h"
#include "envoy/json/json_object.h"
#include "envoy/upstream/cluster_manager.h"

#include "rapidjson/document.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace Envoy {
namespace Http {
namespace Auth {

void AsyncClientCallbacks::onSuccess(MessagePtr &&response) {
  std::string status = response->headers().Status()->value().c_str();
  if (status == "200") {
    std::string body;
    if (response->body()) {
      auto len = response->body()->length();
      body = std::string(static_cast<char *>(response->body()->linearize(len)),
                         len);
    }
    cb_(true, body);
  } else {
    cb_(false, "");
  }
}
void AsyncClientCallbacks::onFailure(AsyncClient::FailureReason) {
  cb_(false, "");
}

void AsyncClientCallbacks::Call(const std::string &uri) {
  // Example:
  // uri  = "https://example.com/certs"
  // pos  :          ^
  // pos1 :                     ^
  // host = "example.com"
  // path = "/certs"
  auto pos = uri.find("://");
  pos = pos == std::string::npos ? 0 : pos + 3;  // Start position of host
  auto pos1 = uri.find("/", pos);
  if (pos1 == std::string::npos) pos1 = uri.length();
  std::string host = uri.substr(pos, pos1 - pos);
  std::string path = "/" + uri.substr(pos1 + 1);

  MessagePtr message(new RequestMessageImpl());
  message->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Get);
  message->headers().insertPath().value(path);
  message->headers().insertHost().value(host);

  cm_.httpAsyncClientForCluster(cluster_->name())
      .send(std::move(message), *this, timeout_);
}

IssuerInfo::IssuerInfo(Json::Object *json) {
  if (json->hasObject("name") && json->hasObject("pubkey")) {
    name_ = json->getString("name");
    auto json_pubkey = json->getObject("pubkey").get();
    if (json_pubkey->hasObject("type")) {
      std::string type = json_pubkey->getString("type");
      pkey_type_ = type;
      if (json_pubkey->hasObject("uri")) {
        // Public key will be loaded from the specified URI.
        uri_ = json_pubkey->getString("uri");
        cluster_ = json_pubkey->hasObject("cluster")
                       ? json_pubkey->getString("cluster")
                       : "";
        return;
      } else if (json_pubkey->hasObject("file")) {
        // Public key is loaded from the specified file.
        std::string path = json_pubkey->getString("file");
        pkey_ = Filesystem::fileReadToEnd(path);
        loaded_ = true;
        return;
      } else if (json_pubkey->hasObject("value")) {
        // Public key is written in this JSON.
        pkey_ = json_pubkey->getString("value");
        loaded_ = true;
        return;
      }
    }
  }
  failed_ = true;
}

/*
 * TODO: add test for config loading
 */
// Load config from envoy config.
void JwtAuthConfig::Load(const Json::Object &json) {
  std::string user_info_type_str =
      json.getString("userinfo_type", "payload_base64url");
  if (user_info_type_str == "payload") {
    user_info_type_ = UserInfoType::kPayload;
  } else if (user_info_type_str == "header_payload_base64url") {
    user_info_type_ = UserInfoType::kHeaderPayloadBase64Url;
  } else {
    user_info_type_ = UserInfoType::kPayloadBase64Url;
  }

  pubkey_cache_expiration_sec_ =
      json.getInteger("pubkey_cache_expiration_sec", 600);

  // Empty array if key "audience" does not exist
  audiences_ = json.getStringArray("audience", true);

  issuers_.clear();
  if (json.hasObject("issuers")) {
    for (auto issuer_json : json.getObjectArray("issuers")) {
      auto issuer = std::make_shared<IssuerInfo>(issuer_json.get());
      issuers_.push_back(issuer);
    }
  }
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
