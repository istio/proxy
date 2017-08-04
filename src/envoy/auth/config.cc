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
  auto len = response->body()->length();
  std::string body(static_cast<char *>(response->body()->linearize(len)), len);
  auto status = std::string(response->headers().Status()->value().c_str());
  if (status == "200") {
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
  message->headers().insertPath().value().append(path.c_str(), path.size());
  message->headers().insertHost().value(host);

  cm_.httpAsyncClientForCluster(cluster_->name())
      .send(std::move(message), *this, timeout_);
}

bool IssuerInfo::Preload(Json::Object *json) {
  if (json->hasObject("name") && json->hasObject("pubkey")) {
    name_ = json->getString("name");
    auto json_pubkey = json->getObject("pubkey").get();
    if (json_pubkey->hasObject("type")) {
      std::string type = json_pubkey->getString("type");
      pkey_type_ = type;
      if (json_pubkey->hasObject("uri")) {
        uri_ = json_pubkey->getString("uri");
        cluster_ = json_pubkey->hasObject("cluster")
                       ? json_pubkey->getString("cluster")
                       : "";
        return true;
      } else if (json_pubkey->hasObject("value")) {
        pkey_ = json_pubkey->getString("value");
        loaded_ = true;
        return true;
      }
    }
  }
  return false;
}

// Load config from envoy config.
void JwtAuthConfig::Load(const Json::Object &json) {
  issuers_.clear();
  if (json.hasObject("issuers")) {
    for (auto issuer_json : json.getObjectArray("issuers")) {
      auto issuer =
          std::shared_ptr<IssuerInfo>(new IssuerInfo(issuer_json.get()));
      issuers_.push_back(issuer);
    }
  }
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy