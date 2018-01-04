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

#include "src/envoy/mixer/stats.h"

namespace Envoy {
namespace Http {
namespace Mixer {

const int MixerStatsObject::kStatsUpdateIntervalInMs = 10000;

MixerStatsObject::MixerStatsObject(const std::string& name, Stats::Scope& scope)
    : stats_{ALL_HTTP_MIXER_FILTER_STATS(POOL_COUNTER_PREFIX(scope, name))} {}

::istio::mixer_client::Statistics* MixerStatsObject::mutate_old_stats() {
  return &old_stats_;
}

void MixerStatsObject::InitGetStatisticsFunc(GetStatsFunc get_stats) {
  get_statistics_ = get_stats;
}

void MixerStatsObject::GetStatistics(::istio::mixer_client::Statistics* stats) {
  if (get_statistics_) {
    get_statistics_(stats);
  }
}

void MixerStatsObject::CheckAndUpdateStats(
    const ::istio::mixer_client::Statistics& new_stats) {
  if (new_stats.total_check_calls > old_stats_.total_check_calls) {
    stats_.total_check_calls_.add(new_stats.total_check_calls -
        old_stats_.total_check_calls);
  }
  if (new_stats.total_remote_check_calls >
      old_stats_.total_remote_check_calls) {
    stats_.total_remote_check_calls_.add(new_stats.total_remote_check_calls -
        old_stats_.total_remote_check_calls);
  }
  if (new_stats.total_blocking_remote_check_calls >
      old_stats_.total_blocking_remote_check_calls) {
    stats_.total_blocking_remote_check_calls_.add(
        new_stats.total_blocking_remote_check_calls -
            old_stats_.total_blocking_remote_check_calls);
  }
  if (new_stats.total_quota_calls > old_stats_.total_quota_calls) {
    stats_.total_quota_calls_.add(new_stats.total_quota_calls -
        old_stats_.total_quota_calls);
  }
  if (new_stats.total_remote_quota_calls >
      old_stats_.total_remote_quota_calls) {
    stats_.total_remote_quota_calls_.add(new_stats.total_remote_quota_calls -
        old_stats_.total_remote_quota_calls);
  }
  if (new_stats.total_blocking_remote_quota_calls >
      old_stats_.total_blocking_remote_quota_calls) {
    stats_.total_blocking_remote_quota_calls_.add(
        new_stats.total_blocking_remote_quota_calls -
            old_stats_.total_blocking_remote_quota_calls);
  }
  if (new_stats.total_report_calls > old_stats_.total_report_calls) {
    stats_.total_report_calls_.add(new_stats.total_report_calls -
        old_stats_.total_report_calls);
  }
  if (new_stats.total_remote_report_calls >
      old_stats_.total_remote_report_calls) {
    stats_.total_remote_report_calls_.add(
        new_stats.total_remote_report_calls -
            old_stats_.total_remote_report_calls);
  }

  // Copy new_stats to old_stats_ for next stats update.
  old_stats_ = new_stats;
}

}  // namespace Mixer
}  // namespace Http
}  // namespace Envoy
