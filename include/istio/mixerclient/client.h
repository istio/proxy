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

#ifndef ISTIO_MIXERCLIENT_CLIENT_H
#define ISTIO_MIXERCLIENT_CLIENT_H

#include <vector>

#include "environment.h"
#include "include/istio/quota_config/requirement.h"
#include "options.h"
#include "src/istio/mixerclient/check_context.h"
#include "src/istio/mixerclient/shared_attributes.h"

namespace istio {
namespace mixerclient {

// Defines the options to create an instance of MixerClient interface.
struct MixerClientOptions {
  // Default constructor with default values.
  MixerClientOptions() {}

  // Constructor with specified option values.
  MixerClientOptions(const CheckOptions& check_options,
                     const ReportOptions& report_options,
                     const QuotaOptions& quota_options)
      : check_options(check_options),
        report_options(report_options),
        quota_options(quota_options) {}

  // Check options.
  CheckOptions check_options;
  // Report options.
  ReportOptions report_options;
  // Quota options.
  QuotaOptions quota_options;
  // The environment functions.
  Environment env;
};

// The statistics recorded by mixerclient library.
struct Statistics {
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

  uint64_t total_check_calls_{0};              // 1.0
  uint64_t total_check_cache_hits_{0};         // 1.1
  uint64_t total_check_cache_misses_{0};       // 1.1
  uint64_t total_check_cache_hit_accepts_{0};  // 1.1
  uint64_t total_check_cache_hit_denies_{0};   // 1.1
  uint64_t total_remote_check_calls_{0};       // 1.0
  uint64_t total_remote_check_accepts_{0};     // 1.1
  uint64_t total_remote_check_denies_{0};      // 1.1

  //
  // Quota check counters
  //
  // total_quota_calls = total_quota_hits + total_quota_misses
  // total_quota_hits = total_quota_hit_accepts + total_quota_hit_denies
  // total_remote_quota_calls = total_quota_misses +
  // total_remote_quota_prefetch_calls total_remote_quota_calls >=
  // total_remote_quota_accepts + total_remote_quota_denies
  //    ^ Transport errors are responsible for the >=
  //

  uint64_t total_quota_calls_{0};                  // 1.0
  uint64_t total_quota_cache_hits_{0};             // 1.1
  uint64_t total_quota_cache_misses_{0};           // 1.1
  uint64_t total_quota_cache_hit_accepts_{0};      // 1.1
  uint64_t total_quota_cache_hit_denies_{0};       // 1.1
  uint64_t total_remote_quota_calls_{0};           // 1.0
  uint64_t total_remote_quota_accepts_{0};         // 1.1
  uint64_t total_remote_quota_denies_{0};          // 1.1
  uint64_t total_remote_quota_prefetch_calls_{0};  // 1.1

  //
  // Counters for upstream requests to Mixer.
  //
  // total_remote_calls = SUM(total_remote_call_successes, ...,
  // total_remote_call_other_errors) Total transport errors would be
  // (total_remote_calls - total_remote_call_successes).
  //

  uint64_t total_remote_calls_{0};               // 1.1
  uint64_t total_remote_call_successes_{0};      // 1.1
  uint64_t total_remote_call_timeouts_{0};       // 1.1
  uint64_t total_remote_call_send_errors_{0};    // 1.1
  uint64_t total_remote_call_other_errors_{0};   // 1.1
  uint64_t total_remote_call_retries_{0};        // 1.1
  uint64_t total_remote_call_cancellations_{0};  // 1.1

  //
  // Telemetry report counters
  //

  // Total number of report calls.
  uint64_t total_report_calls_{0};  // 1.0
  // Total number of remote report calls.
  uint64_t total_remote_report_calls_{0};  // 1.0
  // Remote report calls that succeeed
  uint64_t total_remote_report_successes_{0};  // 1.1
  // Remote report calls that fail due to timeout waiting for the response
  uint64_t total_remote_report_timeouts_{0};  // 1.1
  // Remote report calls that fail sending the request (socket connect or write)
  uint64_t total_remote_report_send_errors_{0};  // 1.1
  // Remote report calls that fail do to some other error
  uint64_t total_remote_report_other_errors_{0};  // 1.1
};

class MixerClient {
 public:
  // Destructor
  virtual ~MixerClient() {}

  // Attribute based calls will be used.
  // Callers should pass in the full set of attributes for the call.
  // The client will use the full set attributes to check cache. If cache
  // miss, an attribute context based on the underlying gRPC stream will
  // be used to generate attribute_update and send that to Mixer server.
  // Callers don't need response data, they only need success or failure.
  // The response data from mixer will be consumed by mixer client.

  // A check call.
  virtual void Check(istio::mixerclient::CheckContextSharedPtr& context,
                     const TransportCheckFunc& transport,
                     const CheckDoneFunc& on_done) = 0;

  // A report call.
  virtual void Report(
      const istio::mixerclient::SharedAttributesSharedPtr& attributes) = 0;

  // Get statistics.
  virtual void GetStatistics(Statistics* stat) const = 0;
};

// Creates a MixerClient object.
std::unique_ptr<MixerClient> CreateMixerClient(
    const MixerClientOptions& options);

}  // namespace mixerclient
}  // namespace istio

#endif  // ISTIO_MIXERCLIENT_CLIENT_H
