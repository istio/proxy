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
//#include <cstring>

namespace Envoy {
namespace Http {
namespace Auth {

void AsyncClientCallbacks::onSuccess(MessagePtr &&response) {
  printf("\n%s\n", __func__);

  printf("\tResponse:\n\n");
  response->headers().iterate(
      [](const HeaderEntry &header, void *) {
        printf("\t%s\t%s\n", header.key().c_str(), header.value().c_str());
      },
      nullptr);

  auto len = response->body()->length();
  std::string body(static_cast<char *>(response->body()->linearize(len)), len);
  printf("\n\tbody:\n\n\tsize=%lu\n\t%s\n\n", len, body.c_str());

  auto status = std::string(response->headers().Status()->value().c_str());
  printf("\tStatus = %s\n", status.c_str());
  if (status == "200") {
    cb_(true, body);
  } else {
    cb_(false, "");
  }
}
void AsyncClientCallbacks::onFailure(AsyncClient::FailureReason /*reason*/) {
  printf("\n%s\n", __func__);

  cb_(false, "");
}

void AsyncClientCallbacks::Call(const std::string &uri) {
  printf("\n%s\n", __func__);

  auto pos = uri.find("://");
  pos = pos == std::string::npos ? 0 : pos + 3;
  auto pos1 = uri.find("/", pos);
  std::string host = uri.substr(pos, pos1 - pos);
  std::string path = uri.substr(pos1);  // "oauth2/v3/certs"

  printf("\n\tCall: uri=%s\n\tcluster->name()=%s\n\tpath=%s\n\thost=%s\n\n",
         uri.c_str(), cluster_->name().c_str(), path.c_str(), host.c_str());

  MessagePtr message(new RequestMessageImpl());
  message->headers().insertMethod().value().setReference(
      Http::Headers::get().MethodValues.Get);
  message->headers().insertPath().value().append(path.c_str(), path.size());
  message->headers().insertHost().value(host);

  printf("\n\tSend Request:\n\n");
  message->headers().iterate(
      [](const HeaderEntry &header, void *) {
        printf("\t%s\t%s\n", header.key().c_str(), header.value().c_str());
      },
      nullptr);

  cm_.httpAsyncClientForCluster(cluster_->name())
      .send(std::move(message), *this, timeout_);
}

std::string IssuerInfo::name() {
  printf("\n%s\n", __func__);
  return name_;
}

std::string IssuerInfo::pkey_type() {
  printf("\n%s\n", __func__);
  return pkey_type_;
}

std::string IssuerInfo::pkey() {
  printf("\n%s\n", __func__);
  return pkey_;
}

bool IssuerInfo::Preload(Json::Object *json) {
  printf("\n%s\n", __func__);
  if (json->hasObject("name") && json->hasObject("pubkey")) {
    name_ = json->getString("name");
    printf("\tname = %s\n\n", name_.c_str());
    auto json_pubkey = json->getObject("pubkey").get();
    if (json_pubkey->hasObject("type")) {
      std::string type = json_pubkey->getString("type");
      printf("\ttype = %s\n\n", type.c_str());
      if (json_pubkey->hasObject("uri")) {
        std::string uri = json_pubkey->getString("uri");
        std::string cluster = json_pubkey->hasObject("cluster")
                                  ? json_pubkey->getString("cluster")
                                  : "";
        uri_ = uri;
        cluster_ = cluster;
        printf("\turi = %s\n\n", uri_.c_str());
        printf("\tcluster = %s\n\n", cluster_.c_str());
        pkey_type_ = type;
        return true;
      } else if (json_pubkey->hasObject("value")) {
        std::string pubkey = json_pubkey->getString("value");
        printf("\tpubkey = %s\n\n", pubkey.c_str());
        pkey_type_ = type;
        pkey_ = pubkey;
        loaded_ = true;

        return true;
      }
    }
  }
  return false;
}

// Load config from envoy config.
void JwtAuthConfig::Load(const Json::Object &json) {
  printf("\n%s\n", __func__);

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