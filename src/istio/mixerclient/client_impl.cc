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
#include "src/istio/mixerclient/client_impl.h"
#include <google/protobuf/arena.h>
#include "include/istio/mixerclient/check_response.h"
#include "include/istio/utils/protobuf.h"

using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::CheckRequest;
using ::istio::mixer::v1::CheckResponse;
using ::istio::mixer::v1::ReportRequest;
using ::istio::mixer::v1::ReportResponse;

namespace istio {
namespace mixerclient {

MixerClientImpl::MixerClientImpl(const MixerClientOptions &options)
    : options_(options) {
  check_cache_ =
      std::unique_ptr<CheckCache>(new CheckCache(options.check_options));
  report_batch_ = std::unique_ptr<ReportBatch>(
      new ReportBatch(options.report_options, options_.env.report_transport,
                      options.env.timer_create_func, compressor_));
  quota_cache_ =
      std::unique_ptr<QuotaCache>(new QuotaCache(options.quota_options));

  if (options_.env.uuid_generate_func) {
    deduplication_id_base_ = options_.env.uuid_generate_func();
  }

  total_check_calls_ = 0;
  total_remote_check_calls_ = 0;
  total_blocking_remote_check_calls_ = 0;
  total_quota_calls_ = 0;
  total_remote_quota_calls_ = 0;
  total_blocking_remote_quota_calls_ = 0;
}

MixerClientImpl::~MixerClientImpl() {}

CancelFunc MixerClientImpl::Check(CheckContextSharedPtr &context,
                                  TransportCheckFunc transport,
                                  CheckDoneFunc on_done) {
  //
  // Always check the policy cache
  //

  context->checkPolicyCache(*check_cache_);
  ++total_check_calls_;

  if (context->policyCacheHit() &&
      (!context->policyStatus().ok() || !context->quotaCheckRequired())) {
    //
    // If the policy cache denies the request, immediately fail the request
    //
    // If policy cache accepts the request and a quota check is not required,
    // immediately accept the request.
    //
    context->setFinalStatus(context->policyStatus());
    on_done(*context);
    return nullptr;
  }

  if (context->quotaCheckRequired()) {
    context->checkQuotaCache(*quota_cache_);
    ++total_quota_calls_;

    if (context->quotaCacheHit() && context->policyCacheHit()) {
      //
      // If both policy and quota caches are hit, we can call the completion
      // handler now.  However sometimes the quota cache's prefetch
      // implementation will still need to send a request to the Mixer server
      // in the background.
      //
      context->setFinalStatus(context->quotaStatus());
      on_done(*context);
      on_done = nullptr;
      if (!context->remoteQuotaRequestRequired()) {
        return nullptr;
      }
    }
  }

  // TODO(jblatt) mjog thinks this is a big CPU hog.  Look into it.
  context->compressRequest(
      compressor_,
      deduplication_id_base_ + std::to_string(deduplication_id_.fetch_add(1)));

  if (!transport) {
    transport = options_.env.check_transport;
  }

  //
  // We are going to make a remote call now.
  //

  ++total_remote_check_calls_;

  if (context->quotaCheckRequired()) {
    ++total_remote_quota_calls_;
  }

  if (on_done) {
    ++total_blocking_remote_check_calls_;
    if (context->quotaCheckRequired()) {
      ++total_blocking_remote_quota_calls_;
    }
  }

  return transport(context->request(), context->response(),
                   [this, context, on_done](const Status &status) {
                     //
                     // Update caches.  This has the side-effect of updating
                     // status, so track those too
                     //

                     if (!context->policyCacheHit()) {
                       context->updatePolicyCache(status, *context->response());
                     }

                     if (context->quotaCheckRequired()) {
                       context->updateQuotaCache(status, *context->response());
                     }

                     //
                     // Determine final status for Filter::completeCheck(). This
                     // will send an error response to the downstream client if
                     // the final status is not Status::OK
                     //

                     if (!status.ok()) {
                       if (context->networkFailOpen()) {
                         context->setFinalStatus(Status::OK);
                       } else {
                         context->setFinalStatus(status);
                       }
                     } else if (!context->quotaCheckRequired()) {
                       context->setFinalStatus(context->policyStatus());
                     } else if (!context->policyStatus().ok()) {
                       context->setFinalStatus(context->policyStatus());
                     } else {
                       context->setFinalStatus(context->quotaStatus());
                     }

                     if (on_done) {
                       on_done(*context);
                     }

                     if (utils::InvalidDictionaryStatus(status)) {
                       compressor_.ShrinkGlobalDictionary();
                     }
                   });
}

void MixerClientImpl::Report(const SharedAttributesSharedPtr &attributes) {
  report_batch_->Report(attributes);
}

void MixerClientImpl::GetStatistics(Statistics *stat) const {
  stat->total_check_calls = total_check_calls_;
  stat->total_remote_check_calls = total_remote_check_calls_;
  stat->total_blocking_remote_check_calls = total_blocking_remote_check_calls_;
  stat->total_quota_calls = total_quota_calls_;
  stat->total_remote_quota_calls = total_remote_quota_calls_;
  stat->total_blocking_remote_quota_calls = total_blocking_remote_quota_calls_;
  stat->total_report_calls = report_batch_->total_report_calls();
  stat->total_remote_report_calls = report_batch_->total_remote_report_calls();
}

// Creates a MixerClient object.
std::unique_ptr<MixerClient> CreateMixerClient(
    const MixerClientOptions &options) {
  return std::unique_ptr<MixerClient>(new MixerClientImpl(options));
}

}  // namespace mixerclient
}  // namespace istio
