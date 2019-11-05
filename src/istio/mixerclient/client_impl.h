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

#ifndef ISTIO_MIXERCLIENT_CLIENT_IMPL_H
#define ISTIO_MIXERCLIENT_CLIENT_IMPL_H

#include <atomic>
#include <random>

#include "include/istio/mixerclient/client.h"
#include "src/istio/mixerclient/attribute_compressor.h"
#include "src/istio/mixerclient/check_cache.h"
#include "src/istio/mixerclient/quota_cache.h"
#include "src/istio/mixerclient/report_batch.h"

using ::istio::mixerclient::CheckContextSharedPtr;
using ::istio::mixerclient::SharedAttributesSharedPtr;

namespace istio {
namespace mixerclient {

class MixerClientImpl : public MixerClient {
 public:
  // Constructor
  MixerClientImpl(const MixerClientOptions& options);

  // Destructor
  virtual ~MixerClientImpl();

  void Check(CheckContextSharedPtr& context,
             const TransportCheckFunc& transport,
             const CheckDoneFunc& on_done) override;

  void Report(const SharedAttributesSharedPtr& attributes) override;

  void GetStatistics(Statistics* stat) const override;

 private:
  void RemoteCheck(CheckContextSharedPtr context,
                   const TransportCheckFunc& transport,
                   const CheckDoneFunc& on_done);

  uint32_t RetryDelay(uint32_t retry_attempt);

  // Store the options
  MixerClientOptions options_;

  // To compress attributes.
  AttributeCompressor compressor_;

  // timer create func
  TimerCreateFunc timer_create_;
  // Cache for Check call.
  std::unique_ptr<CheckCache> check_cache_;
  // Report batch.
  std::shared_ptr<ReportBatch> report_batch_;
  // Cache for Quota call.
  std::unique_ptr<QuotaCache> quota_cache_;

  // RNG for retry jitter
  std::default_random_engine rand_;

  // for deduplication_id
  std::string deduplication_id_base_;
  std::atomic<std::uint64_t> deduplication_id_;

  //
  // Policy check counters.
  //
  // total_check_calls = total_check_hits + total_check_misses
  // total_check_hits = total_check_hit_accepts + total_check_hit_denies
  // total_remote_check_calls = total_check_misses
  // total_remote_check_calls >= total_remote_check_accepts +
  // total_remote_check_denies
  //    ^ Transport errors are responsible for the >=
  //

  std::atomic<uint64_t> total_check_calls_{0};              // 1.0
  std::atomic<uint64_t> total_check_cache_hits_{0};         // 1.1
  std::atomic<uint64_t> total_check_cache_misses_{0};       // 1.1
  std::atomic<uint64_t> total_check_cache_hit_accepts_{0};  // 1.1
  std::atomic<uint64_t> total_check_cache_hit_denies_{0};   // 1.1
  std::atomic<uint64_t> total_remote_check_calls_{0};       // 1.0
  std::atomic<uint64_t> total_remote_check_accepts_{0};     // 1.1
  std::atomic<uint64_t> total_remote_check_denies_{0};      // 1.1

  //
  // Quota check counters
  //
  // total_quota_calls = total_quota_hits + total_quota_misses
  // total_quota_hits >= total_quota_hit_accepts + total_quota_hit_denies
  //    ^ we will neither accept or deny from the quota cache if the policy
  //    cache is missed
  // total_remote_quota_calls = total_quota_misses + total_quota_hit_denies
  //    ^ we will neither accept or deny from the quota cache if the policy
  //    cache is missed
  // total_remote_quota_calls >= total_remote_quota_accepts +
  // total_remote_quota_denies
  //    ^ Transport errors are responsible for the >=
  //

  std::atomic<uint64_t> total_quota_calls_{0};                  // 1.0
  std::atomic<uint64_t> total_quota_cache_hits_{0};             // 1.1
  std::atomic<uint64_t> total_quota_cache_misses_{0};           // 1.1
  std::atomic<uint64_t> total_quota_cache_hit_accepts_{0};      // 1.1
  std::atomic<uint64_t> total_quota_cache_hit_denies_{0};       // 1.1
  std::atomic<uint64_t> total_remote_quota_calls_{0};           // 1.0
  std::atomic<uint64_t> total_remote_quota_accepts_{0};         // 1.1
  std::atomic<uint64_t> total_remote_quota_denies_{0};          // 1.1
  std::atomic<uint64_t> total_remote_quota_prefetch_calls_{0};  // 1.1

  //
  // Counters for upstream requests to Mixer.
  //
  // total_remote_calls = SUM(total_remote_call_successes, ...,
  // total_remote_call_other_errors) Total transport errors would be
  // (total_remote_calls - total_remote_call_successes).
  //

  std::atomic<uint64_t> total_remote_calls_{0};               // 1.1
  std::atomic<uint64_t> total_remote_call_successes_{0};      // 1.1
  std::atomic<uint64_t> total_remote_call_timeouts_{0};       // 1.1
  std::atomic<uint64_t> total_remote_call_send_errors_{0};    // 1.1
  std::atomic<uint64_t> total_remote_call_other_errors_{0};   // 1.1
  std::atomic<uint64_t> total_remote_call_retries_{0};        // 1.1
  std::atomic<uint64_t> total_remote_call_cancellations_{0};  // 1.1

  GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(MixerClientImpl);
};

}  // namespace mixerclient
}  // namespace istio

#endif  // ISTIO_MIXERCLIENT_CLIENT_IMPL_H
