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
using ::istio::mixerclient::CheckContextSharedPtr;
using ::istio::mixerclient::SharedAttributesSharedPtr;

namespace istio {
namespace mixerclient {

static ::google::protobuf::StringPiece TIMEOUT_MESSAGE(
    "upstream request timeout");
static ::google::protobuf::StringPiece SEND_ERROR_MESSAGE(
    "upstream connect error or disconnect/reset before headers");

enum class TransportResult {
  SUCCESS,           // Response received
  SEND_ERROR,        // Cannot connect to peer or send request to peer.
  RESPONSE_TIMEOUT,  // Connected to peer and sent request, but didn't receive a
                     // response in time.
  OTHER              // Something else went wrong
};

TransportResult TransportStatus(const Status &status) {
  if (status.ok()) {
    return TransportResult::SUCCESS;
  }

  if (Code::UNAVAILABLE == status.error_code()) {
    if (TIMEOUT_MESSAGE == status.error_message()) {
      return TransportResult::RESPONSE_TIMEOUT;
    }
    if (SEND_ERROR_MESSAGE == status.error_message()) {
      return TransportResult::SEND_ERROR;
    }
  }

  return TransportResult::OTHER;
}

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

  if (context->policyCacheHit()) {
    ++total_check_cache_hits_;

    if (!context->policyStatus().ok()) {
      //
      // If the policy cache denies the request, immediately fail the request
      //
      ++total_check_cache_hit_denies_;
      context->setFinalStatus(context->policyStatus());
      on_done(*context);
      return nullptr;
    }

    //
    // If policy cache accepts the request and a quota check is not required,
    // immediately accept the request.
    //
    ++total_check_cache_hit_accepts_;
    if (!context->quotaCheckRequired()) {
      context->setFinalStatus(context->policyStatus());
      on_done(*context);
      return nullptr;
    }
  } else {
    ++total_check_cache_misses_;
  }

  if (context->quotaCheckRequired()) {
    context->checkQuotaCache(*quota_cache_);
    ++total_quota_calls_;

    if (context->quotaCacheHit()) {
      ++total_quota_cache_hits_;
      if (context->quotaStatus().ok()) {
        ++total_quota_cache_hit_accepts_;
      } else {
        ++total_quota_cache_hit_denies_;
      }

      if (context->policyCacheHit()) {
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
        } else {
          ++total_remote_quota_prefetch_calls_;
        }
      }
    } else {
      ++total_quota_cache_misses_;
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
  // Classify and track reason for remote request
  //

  ++total_remote_calls_;

  if (!context->policyCacheHit()) {
    ++total_remote_check_calls_;
  }

  if (context->remoteQuotaRequestRequired()) {
    ++total_remote_quota_calls_;
  }

  return transport(context->request(), context->response(),
                   [this, context, on_done](const Status &status) {
                     //
                     // Classify and track transport errors
                     //

                     TransportResult result = TransportStatus(status);

                     switch (result) {
                       case TransportResult::SUCCESS:
                         ++total_remote_call_successes_;
                         break;
                       case TransportResult::RESPONSE_TIMEOUT:
                         ++total_remote_call_timeouts_;
                         break;
                       case TransportResult::SEND_ERROR:
                         ++total_remote_call_send_errors_;
                         break;
                       case TransportResult::OTHER:
                         ++total_remote_call_other_errors_;
                         break;
                     }

                     //
                     // Update caches.  This has the side-effect of updating
                     // status, so track those too
                     //

                     if (!context->policyCacheHit()) {
                       context->updatePolicyCache(status, *context->response());

                       if (context->policyStatus().ok()) {
                         ++total_remote_check_accepts_;
                       } else {
                         ++total_remote_check_denies_;
                       }
                     }

                     if (context->quotaCheckRequired()) {
                       context->updateQuotaCache(status, *context->response());

                       if (context->quotaStatus().ok()) {
                         ++total_remote_quota_accepts_;
                       } else {
                         ++total_remote_quota_denies_;
                       }
                     }

                     //
                     // Determine final status for Filter::completeCheck(). This
                     // will send an error response to the downstream client if
                     // the final status is not Status::OK
                     //

                     if (result != TransportResult::SUCCESS) {
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
  stat->total_check_calls_ = total_check_calls_;
  stat->total_check_cache_hits_ = total_check_cache_hits_;
  stat->total_check_cache_misses_ = total_check_cache_misses_;
  stat->total_check_cache_hit_accepts_ = total_check_cache_hit_accepts_;
  stat->total_check_cache_hit_denies_ = total_check_cache_hit_denies_;
  stat->total_remote_check_calls_ = total_remote_check_calls_;
  stat->total_remote_check_accepts_ = total_remote_check_accepts_;
  stat->total_remote_check_denies_ = total_remote_check_denies_;
  stat->total_quota_calls_ = total_quota_calls_;
  stat->total_quota_cache_hits_ = total_quota_cache_hits_;
  stat->total_quota_cache_misses_ = total_quota_cache_misses_;
  stat->total_quota_cache_hit_accepts_ = total_quota_cache_hit_accepts_;
  stat->total_quota_cache_hit_denies_ = total_quota_cache_hit_denies_;
  stat->total_remote_quota_calls_ = total_remote_quota_calls_;
  stat->total_remote_quota_accepts_ = total_remote_quota_accepts_;
  stat->total_remote_quota_denies_ = total_remote_quota_denies_;
  stat->total_remote_quota_prefetch_calls_ = total_remote_quota_prefetch_calls_;
  stat->total_remote_calls_ = total_remote_calls_;
  stat->total_remote_call_successes_ = total_remote_call_successes_;
  stat->total_remote_call_timeouts_ = total_remote_call_timeouts_;
  stat->total_remote_call_send_errors_ = total_remote_call_send_errors_;
  stat->total_remote_call_other_errors_ = total_remote_call_other_errors_;
  stat->total_remote_call_retries_ = total_remote_call_retries_;
  stat->total_remote_call_cancellations_ = total_remote_call_cancellations_;

  stat->total_report_calls_ = report_batch_->total_report_calls();
  stat->total_remote_report_calls_ = report_batch_->total_remote_report_calls();
}

// Creates a MixerClient object.
std::unique_ptr<MixerClient> CreateMixerClient(
    const MixerClientOptions &options) {
  return std::unique_ptr<MixerClient>(new MixerClientImpl(options));
}

}  // namespace mixerclient
}  // namespace istio
