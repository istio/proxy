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

#include "src/istio/control/http/request_handler_impl.h"

#include "src/istio/control/http/attributes_builder.h"

using ::google::protobuf::util::Status;
using ::istio::mixerclient::CancelFunc;
using ::istio::mixerclient::CheckDoneFunc;
using ::istio::mixerclient::CheckResponseInfo;
using ::istio::mixerclient::TimerCreateFunc;
using ::istio::mixerclient::TransportCheckFunc;
using ::istio::quota_config::Requirement;

namespace istio {
namespace control {
namespace http {

RequestHandlerImpl::RequestHandlerImpl(
    std::shared_ptr<ServiceContext> service_context)
    : attributes_(new istio::mixerclient::SharedAttributes()),
      check_context_(new istio::mixerclient::CheckContext(
          service_context->client_context()->Retries(),
          service_context->client_context()->NetworkFailOpen(), attributes_)),
      service_context_(service_context) {}

void RequestHandlerImpl::AddForwardAttributes(CheckData* check_data) {
  if (forward_attributes_added_) {
    return;
  }
  forward_attributes_added_ = true;

  if (!service_context_->ignore_forwarded_attributes()) {
    AttributesBuilder builder(attributes_->attributes());
    builder.ExtractForwardedAttributes(check_data);
  }
}

void RequestHandlerImpl::AddCheckAttributes(CheckData* check_data) {
  if (check_attributes_added_) {
    return;
  }
  check_attributes_added_ = true;

  if (service_context_->enable_mixer_check() ||
      service_context_->enable_mixer_report()) {
    service_context_->AddStaticAttributes(attributes_->attributes());

    AttributesBuilder builder(attributes_->attributes());
    builder.ExtractCheckAttributes(check_data);
  }
}

void RequestHandlerImpl::Check(CheckData* check_data,
                               HeaderUpdate* header_update,
                               const TransportCheckFunc& transport,
                               const CheckDoneFunc& on_done) {
  // Forwarded attributes need to be stored regardless Check is needed
  // or not since the header will be updated or removed.
  AddCheckAttributes(check_data);
  AddForwardAttributes(check_data);
  header_update->RemoveIstioAttributes();
  service_context_->InjectForwardedAttributes(header_update);

  if (!service_context_->enable_mixer_check()) {
    check_context_->setFinalStatus(Status::OK, false);
    on_done(*check_context_);
    return;
  }

  service_context_->AddQuotas(attributes_->attributes(),
                              check_context_->quotaRequirements());

  service_context_->client_context()->SendCheck(transport, on_done,
                                                check_context_);
}

void RequestHandlerImpl::ResetCancel() {
  if (check_context_) {
    check_context_->resetCancel();
  }
}

void RequestHandlerImpl::CancelCheck() {
  if (check_context_) {
    check_context_->cancel();
  }
}

// Make remote report call.
void RequestHandlerImpl::Report(CheckData* check_data,
                                ReportData* report_data) {
  if (!service_context_->enable_mixer_report()) {
    return;
  }

  AddForwardAttributes(check_data);
  AddCheckAttributes(check_data);

  AttributesBuilder builder(attributes_->attributes());
  builder.ExtractReportAttributes(check_context_->status(), report_data);

  service_context_->client_context()->SendReport(attributes_);
}

}  // namespace http
}  // namespace control
}  // namespace istio
