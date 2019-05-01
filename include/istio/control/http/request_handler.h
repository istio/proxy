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

#ifndef ISTIO_CONTROL_HTTP_REQUEST_HANDLER_H
#define ISTIO_CONTROL_HTTP_REQUEST_HANDLER_H

#include "include/istio/control/http/check_data.h"
#include "include/istio/control/http/report_data.h"
#include "include/istio/mixerclient/client.h"

namespace istio {
namespace control {
namespace http {

// The interface to handle a HTTP request.
class RequestHandler {
 public:
  virtual ~RequestHandler() {}

  // Perform a Check call. It will:
  // * extract forwarded attributes from client proxy
  // * extract attributes from the request
  // * extract attributes from the config.
  // * if necessary, forward some attributes to downstream
  // * make a Check call.
  virtual void Check(CheckData* check_data, HeaderUpdate* header_update,
                     const ::istio::mixerclient::TransportCheckFunc& transport,
                     const ::istio::mixerclient::CheckDoneFunc& on_done) = 0;

  virtual void ResetCancel() = 0;

  virtual void CancelCheck() = 0;

  // Make a Report call. It will:
  // * check service config to see if Report is required
  // * extract check attributes if not done yet.
  // * extract more report attributes
  // * make a Report call.
  virtual void Report(CheckData* check_data, ReportData* report_data) = 0;
};

}  // namespace http
}  // namespace control
}  // namespace istio

#endif  // ISTIO_CONTROL_HTTP_REQUEST_HANDLER_H
