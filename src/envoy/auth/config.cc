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

#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"
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

IssuerInfo::IssuerInfo(const std::string &name, const std::string &pkey_type,
                       const std::string &pkey) {
  name_ = name;
  pkey_type_ = pkey_type;
  pkey_ = pkey;
}

class Callbacks : public AsyncClient::Callbacks {
 public:
  Callbacks(Upstream::ClusterManager &cm, const std::string &cluster)
      : cm_(cm),
        cluster_(cm.get(cluster)->info()),
        timeout_(Optional<std::chrono::milliseconds>()) {}
  void onSuccess(MessagePtr &&response) {
    printf("\nonSuccess\n");
    printf("Response:\n");
    response->headers().iterate(
        [](const HeaderEntry &header, void *) {
          printf("\t%s\t%s\n", header.key().c_str(), header.value().c_str());
        },
        nullptr);

    auto len = response->body()->length();
    std::string body(static_cast<char *>(response->body()->linearize(len)),
                     len);
    printf("body:\n\tsize=%lu\n\t%s\n\n", len, body.c_str());
  }
  void onFailure(AsyncClient::FailureReason /*reason*/) {
    printf("\n\tonFailure\n\n");
  }
  void Call(const std::string &uri) {
    auto pos = uri.find("://");
    pos = pos == std::string::npos ? 0 : pos + 3;
    auto pos1 = uri.find("/", pos);
    std::string host = uri.substr(pos, pos1 - pos);
    std::string path = uri.substr(pos1);  // "oauth2/v3/certs"
                                          //    path = uri;

    printf("\n\tCall: uri=%s\n\tcluster->name()=%s\n\tpath=%s\n\thost=%s\n\n",
           uri.c_str(), cluster_->name().c_str(), path.c_str(), host.c_str());

    MessagePtr message(new RequestMessageImpl());
    message->headers().insertMethod().value().setReference(
        Http::Headers::get().MethodValues.Get);
    message->headers().insertPath().value().append(path.c_str(), path.size());
    message->headers().insertHost().value(host);

    printf("\nSend Request:\n\n");
    message->headers().iterate(
        [](const HeaderEntry &header, void *) {
          printf("\t%s\t%s\n", header.key().c_str(), header.value().c_str());
        },
        nullptr);

    //    Http::AsyncClient::Request* http_request =
    cm_.httpAsyncClientForCluster(cluster_->name())
        .send(std::move(message), *this, timeout_);
  }

 private:
  Upstream::ClusterManager &cm_;
  Upstream::ClusterInfoConstSharedPtr cluster_;
  Optional<std::chrono::milliseconds> timeout_;
};

std::string JwtAuthConfig::ReadWholeFile(const std::string &path) {
  std::ifstream ifs(path.c_str());
  printf("\n\tpath:\t%s\n\n", path.c_str());
  if (!ifs) {
    printf("\n\tifs open error\n\n");
    return "";
  }
  return std::string((std::istreambuf_iterator<char>(ifs)),
                     (std::istreambuf_iterator<char>()));
}

std::string JwtAuthConfig::ReadWholeFileByHttp(const std::string &uri,
                                               const std::string &cluster) {
  printf("\n\tReadWholeFilebyHttp\n\n");
  Callbacks cb(cm_, cluster);
  cb.Call(uri);
  /*
   * TODO: implement
   */
  return "";
}

std::string JwtAuthConfig::GetContentFromUri(const std::string &uri,
                                             const std::string &cluster) {
  printf("\n\tGetContentFromUri: uri=%s\tcluster=%s\n\n", uri.c_str(),
         cluster.c_str());

  const std::string http = "http";
  if (strncmp(uri.c_str(), http.c_str(), http.length()) == 0) {
    return ReadWholeFileByHttp(uri, cluster);
  } else {
    return ReadWholeFile(uri);
  }
  return "";
}

std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadIssuerFromDiscoveryDocumentStr(
    const std::string &doc, const std::string &cluster) {
  rapidjson::Document json;
  if (json.Parse(doc.c_str()).HasParseError()) {
    return nullptr;
  }
  if (!json.HasMember("issuer") || !json["issuer"].IsString()) {
    return nullptr;
  }
  std::string name = json["issuer"].GetString();
  if (!json.HasMember("jwks_uri") || !json["jwks_uri"].IsString()) {
    return nullptr;
  }
  std::string jwks_uri = json["jwks_uri"].GetString();
  std::string jwks = GetContentFromUri(jwks_uri, cluster);
  return std::shared_ptr<IssuerInfo>(new IssuerInfo(name, "jwks", jwks));
}

std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadIssuerFromDiscoveryDocument(
    Json::Object *json) {
  if (json->hasObject("uri")) {
    std::string uri = json->getString("uri");
    std::string cluster =
        json->hasObject("cluster") ? json->getString("cluster") : "";
    std::string discovery_document = GetContentFromUri(uri, cluster);
    std::string jwks_cluster =
        json->hasObject("jwks_cluster") ? json->getString("jwks_cluster") : "";
    return LoadIssuerFromDiscoveryDocumentStr(discovery_document, jwks_cluster);
  } else if (json->hasObject("value")) {
    std::string discovery_document = json->getString("value");
    std::string jwks_cluster =
        json->hasObject("jwks_cluster") ? json->getString("jwks_cluster") : "";
    return LoadIssuerFromDiscoveryDocumentStr(discovery_document, jwks_cluster);
  }
  return nullptr;
}

std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadPubkeyFromObject(
    Json::Object *json) {
  if (json->hasObject("type")) {
    std::string type = json->getString("type");
    if (json->hasObject("uri")) {
      std::string uri = json->getString("uri");
      std::string cluster =
          json->hasObject("cluster") ? json->getString("cluster") : "";
      std::string pubkey = GetContentFromUri(uri, cluster);
      return std::shared_ptr<IssuerInfo>(new IssuerInfo("", type, pubkey));
    } else if (json->hasObject("value")) {
      std::string pubkey = json->getString("value");
      return std::shared_ptr<IssuerInfo>(new IssuerInfo("", type, pubkey));
    }
  }
  return nullptr;
}

// Load information of an issuer. Returns nullptr if bad-formatted.
std::shared_ptr<IssuerInfo> JwtAuthConfig::LoadIssuer(Json::Object *json) {
  if (json->hasObject("discovery_document")) {
    return LoadIssuerFromDiscoveryDocument(
        json->getObject("discovery_document").get());
  } else {
    if (json->hasObject("name") && json->hasObject("pubkey")) {
      auto ret = LoadPubkeyFromObject(json->getObject("pubkey").get());
      ret->name_ = json->getString("name");
      return ret;
    }
  }
  return nullptr;
}

// Load config from envoy config.
void JwtAuthConfig::Load(const Json::Object &json) {
  issuers_.clear();
  if (json.hasObject("issuers")) {
    for (auto issuer_json : json.getObjectArray("issuers")) {
      auto issuer = LoadIssuer(issuer_json.get());
      if (issuer) {
        issuers_.push_back(issuer);
      }
    }
  }
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy