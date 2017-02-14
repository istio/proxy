// Copyright 2017 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////////
// includes should be ordered. This seems like a bug in clang-format?
#include <iostream>

#include "contrib/endpoints/src/api_manager/cloud_trace/cloud_trace.h"
#include "contrib/endpoints/src/api_manager/quota_control.h"
#include "google/protobuf/stubs/status.h"

using ::google::api_manager::utils::Status;
using ::google::protobuf::util::error::Code;

namespace google {
namespace api_manager {

void QuotaControl(std::shared_ptr<context::RequestContext> context,
                  std::function<void(Status status)> continuation) {
  std::shared_ptr<cloud_trace::CloudTraceSpan> trace_span(
      CreateSpan(context->cloud_trace(), "QuotaControl"));

  if (!context->service_context()->service_control()) {
    TRACE(trace_span) << "Service control check is not needed";
    continuation(Status::OK);
    return;
  }

  if (context->api_key().empty()) {
    if (context->method()->allow_unregistered_calls()) {
      // Not need to call Check.
      TRACE(trace_span) << "Service control check is not needed";
      continuation(Status::OK);
      return;
    }

    TRACE(trace_span) << "Failed at checking caller identity.";
    continuation(
        Status(Code::UNAUTHENTICATED,
               "Method doesn't allow unregistered callers (callers without "
               "established identity). Please use API Key or other form of "
               "API consumer identity to call this API.",
               Status::SERVICE_CONTROL));
    return;
  }


  service_control::QuotaRequestInfo info;
  context->FillAllocateQuotaRequestInfo(&info);
  context->service_context()->service_control()->Quota(
      info, trace_span.get(),
      [context, continuation, trace_span](
          utils::Status status,
          const service_control::QuotaResponseInfo &info) {

    TRACE(trace_span) << "Quota service control request returned with "
                          << "status " << status.ToString();

        // info is valid regardless status.
        context->set_quota_response_info(info);
        continuation(status);
      });
}

}  // namespace service_control_client
}  // namespace google
