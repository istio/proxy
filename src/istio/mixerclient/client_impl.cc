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

#include <algorithm>
#include <cmath>

#include "include/istio/mixerclient/check_response.h"
#include "include/istio/utils/protobuf.h"
#include "src/istio/mixerclient/status_util.h"
#include "src/istio/utils/logger.h"

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

MixerClientImpl::MixerClientImpl(const MixerClientOptions &options)
    : options_(options) {
  timer_create_ = options.env.timer_create_func;
  check_cache_ =
      std::unique_ptr<CheckCache>(new CheckCache(options.check_options));
  report_batch_ = std::shared_ptr<ReportBatch>(
      new ReportBatch(options.report_options, options_.env.report_transport,
                      timer_create_, compressor_));
  quota_cache_ =
      std::unique_ptr<QuotaCache>(new QuotaCache(options.quota_options));

  if (options_.env.uuid_generate_func) {
    deduplication_id_base_ = options_.env.uuid_generate_func();
  }
}

MixerClientImpl::~MixerClientImpl() {
  if (report_batch_) {
    report_batch_->Flush();
    report_batch_.reset();
  }
}

uint32_t MixerClientImpl::RetryDelay(uint32_t retry_attempt) {
  const uint32_t max_retry_ms =
      std::min(options_.check_options.max_retry_ms,
               options_.check_options.base_retry_ms *
                   static_cast<uint32_t>(std::pow(2, retry_attempt)));

  std::uniform_int_distribution<uint32_t> distribution(
      options_.check_options.base_retry_ms, max_retry_ms);

  return distribution(rand_);
}

void MixerClientImpl::Check(CheckContextSharedPtr &context,
                            const TransportCheckFunc &transport,
                            const CheckDoneFunc &on_done) {
  //
  // Always check the policy cache
  //

  context->checkPolicyCache(*check_cache_);
  ++total_check_calls_;

  MIXER_DEBUG("Policy cache hit=%s, status=%s",
              context->policyCacheHit() ? "true" : "false",
              context->policyStatus().ToString().c_str());

  if (context->policyCacheHit()) {
    ++total_check_cache_hits_;

    if (!context->policyStatus().ok()) {
      //
      // If the policy cache denies the request, immediately fail the request
      //
      ++total_check_cache_hit_denies_;
      context->setFinalStatus(context->policyStatus());
      on_done(*context);
      return;
    }

    //
    // If policy cache accepts the request and a quota check is not required,
    // immediately accept the request.
    //
    ++total_check_cache_hit_accepts_;
    if (!context->quotaCheckRequired()) {
      context->setFinalStatus(context->policyStatus());
      on_done(*context);
      return;
    }
  } else {
    ++total_check_cache_misses_;
  }

  bool remote_quota_prefetch{false};

  if (context->quotaCheckRequired()) {
    context->checkQuotaCache(*quota_cache_);
    ++total_quota_calls_;

    MIXER_DEBUG("Quota cache hit=%s, status=%s, remote_call=%s",
                context->quotaCacheHit() ? "true" : "false",
                context->quotaStatus().ToString().c_str(),
                context->remoteQuotaRequestRequired() ? "true" : "false");

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
        remote_quota_prefetch = context->remoteQuotaRequestRequired();
        if (!remote_quota_prefetch) {
          return;
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

  if (remote_quota_prefetch) {
    ++total_remote_quota_prefetch_calls_;
  }

  RemoteCheck(context, transport ? transport : options_.env.check_transport,
              remote_quota_prefetch ? nullptr : on_done);
}

void MixerClientImpl::RemoteCheck(CheckContextSharedPtr context,
                                  const TransportCheckFunc &transport,
                                  const CheckDoneFunc &on_done) {
  //
  // This lambda and any lambdas it creates for retry will inc the ref count
  // on the CheckContext shared pointer.
  //
  // The CheckDoneFunc is valid as long as the Filter object is valid.  This
  // has a lifespan similar to the CheckContext, but TODO(jblatt) it would be
  // good to move this into the CheckContext anyways.
  //
  // The other captures (this/MixerClientImpl and TransportCheckFunc's
  // references) have lifespans much greater than any individual transaction.
  //
  CancelFunc cancel_func = transport(
      context->request(), context->response(),
      [this, context, transport, on_done](const Status &status) {
        context->resetCancel();

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

        if (result != TransportResult::SUCCESS && context->retryable()) {
          ++total_remote_call_retries_;
          const uint32_t retry_ms = RetryDelay(context->retryAttempt());

          MIXER_DEBUG("Retry %u in %u msec due to transport error=%s",
                      context->retryAttempt() + 1, retry_ms,
                      status.ToString().c_str());

          context->retry(retry_ms,
                         timer_create_([this, context, transport, on_done]() {
                           RemoteCheck(context, transport, on_done);
                         }));

          return;
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

        MIXER_DEBUG(
            "CheckResult transport=%s, policy=%s, quota=%s, attempt=%u",
            status.ToString().c_str(),
            result == TransportResult::SUCCESS
                ? context->policyStatus().ToString().c_str()
                : "NA",
            result == TransportResult::SUCCESS && context->quotaCheckRequired()
                ? context->policyStatus().ToString().c_str()
                : "NA",
            context->retryAttempt());

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
          // TODO(jblatt) verify this is threadsafe
          compressor_.ShrinkGlobalDictionary();
        }
      });

  context->setCancel([this, cancel_func]() {
    ++total_remote_call_cancellations_;
    cancel_func();
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
  stat->total_remote_report_successes_ =
      report_batch_->total_remote_report_successes();
  stat->total_remote_report_timeouts_ =
      report_batch_->total_remote_report_timeouts();
  stat->total_remote_report_send_errors_ =
      report_batch_->total_remote_report_send_errors();
  stat->total_remote_report_other_errors_ =
      report_batch_->total_remote_report_other_errors();
}

// Creates a MixerClient object.
std::unique_ptr<MixerClient> CreateMixerClient(
    const MixerClientOptions &options) {
  return std::unique_ptr<MixerClient>(new MixerClientImpl(options));
}

}  // namespace mixerclient
}  // namespace istio
