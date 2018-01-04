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

#pragma once

#include "common/common/logger.h"
#include "envoy/stats/stats_macros.h"
#include "include/client.h"

namespace Envoy {
namespace Http {
namespace Mixer {

/**
 * All http mixer filter stats. @see stats_macros.h
 */
// clang-format off
#define ALL_HTTP_MIXER_FILTER_STATS(COUNTER)                                  \
  COUNTER(total_check_calls)                                                  \
  COUNTER(total_remote_check_calls)                                           \
  COUNTER(total_blocking_remote_check_calls)                                  \
  COUNTER(total_quota_calls)                                                  \
  COUNTER(total_remote_quota_calls)                                           \
  COUNTER(total_blocking_remote_quota_calls)                                  \
  COUNTER(total_report_calls)                                                 \
  COUNTER(total_remote_report_calls)
// clang-format on

/**
 * Struct definition for all mixer filter stats. @see stats_macros.h
 */
struct MixerFilterStats {
  ALL_HTTP_MIXER_FILTER_STATS(GENERATE_COUNTER_STRUCT)
};

typedef std::function<void(::istio::mixer_client::Statistics* s)> GetStatsFunc;

// MixerStatsObject maintains statistics for number of check, quota and report
// calls issued by a mixer filter.
class MixerStatsObject {
 public:
  static const int kStatsUpdateIntervalInMs;

  MixerStatsObject(const std::string& name, Stats::Scope& scope);

  void CheckAndUpdateStats(const ::istio::mixer_client::Statistics& new_stats);

  ::istio::mixer_client::Statistics* mutate_old_stats();

  void InitGetStatisticsFunc(GetStatsFunc get_stats);

  void GetStatistics(::istio::mixer_client::Statistics* stats);

 private:
  MixerFilterStats stats_;
  GetStatsFunc get_statistics_;
  // stats from last call to MixerClient::GetStatistics(). This is needed to
  // calculate the variances of stats and update envoy stats.
  ::istio::mixer_client::Statistics old_stats_;
};

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
