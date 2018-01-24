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

#include "http_request.h"

#include "common/common/logger.h"
#include "common/http/message_impl.h"
#include "envoy/http/async_client.h"

namespace Envoy {
namespace Http {
namespace Auth {
namespace {

// Extract host and path from a URI
void ExtractUriHostPath(const std::string& uri, std::string* host,
                        std::string* path) {
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
  *host = uri.substr(pos, pos1 - pos);
  *path = "/" + uri.substr(pos1 + 1);
}

// Callback class for AsyncClient to use Envoy to make remote HTTP call.
class AsyncClientCallbacks : public AsyncClient::Callbacks,
                             public Logger::Loggable<Logger::Id::http> {
 public:
  AsyncClientCallbacks(Upstream::ClusterManager& cm, const std::string& uri,
                       const std::string& cluster, HttpDoneFunc cb)
      : uri_(uri), cb_(cb) {
    std::string host, path;
    ExtractUriHostPath(uri, &host, &path);

    MessagePtr message(new RequestMessageImpl());
    message->headers().insertMethod().value().setReference(
        Http::Headers::get().MethodValues.Get);
    message->headers().insertPath().value(path);
    message->headers().insertHost().value(host);

    request_ = cm.httpAsyncClientForCluster(cluster).send(
        std::move(message), *this, Optional<std::chrono::milliseconds>());
  }

  void onSuccess(MessagePtr&& response) {
    std::string status = response->headers().Status()->value().c_str();
    if (status == "200") {
      ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: success", uri_);
      std::string body;
      if (response->body()) {
        auto len = response->body()->length();
        body = std::string(static_cast<char*>(response->body()->linearize(len)),
                           len);
      } else {
        ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: body is empty",
                  uri_);
      }
      cb_(true, body);
    } else {
      ENVOY_LOG(debug,
                "AsyncClientCallbacks [uri = {}]: response status code {}",
                uri_, status);
      cb_(false, "");
    }
    delete this;
  }

  void onFailure(AsyncClient::FailureReason) {
    ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: failed", uri_);
    cb_(false, "");
    delete this;
  }

  void Cancel() {
    ENVOY_LOG(debug, "AsyncClientCallbacks [uri = {}]: canceled", uri_);
    request_->cancel();
    delete this;
  }

 private:
  const std::string& uri_;
  HttpDoneFunc cb_;
  AsyncClient::Request* request_;
};

}  // namespace

HttpGetFunc NewHttpGetFuncByAsyncClient(Upstream::ClusterManager& cm) {
  return [&cm](const std::string& uri, const std::string& cluster,
               HttpDoneFunc http_done) -> CancelFunc {
    auto transport = new AsyncClientCallbacks(cm, uri, cluster, http_done);
    return [transport]() { transport->Cancel(); };
  };
}

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy
