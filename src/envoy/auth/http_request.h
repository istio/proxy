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

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <functional>
#include "envoy/upstream/cluster_manager.h"

namespace Envoy {
namespace Http {
namespace Auth {

// The callback function after HTTP fetch call is done.
using HttpDoneFunc = std::function<void(bool ok, const std::string& body)>;
// The function to cancel the pending remote call.
using CancelFunc = std::function<void()>;

// The HTTP Get call interface.
using HttpGetFunc =
    std::function<CancelFunc(const std::string& url, const std::string& cluster,
                             HttpDoneFunc http_done)>;

// Create a HTTPGetFunc by using Envoy async client.
HttpGetFunc NewHttpGetFuncByAsyncClient(Upstream::ClusterManager& cm);

}  // namespace Auth
}  // namespace Http
}  // namespace Envoy

#endif  // HTTP_REQUEST_H
