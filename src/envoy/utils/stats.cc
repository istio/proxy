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

#include "src/envoy/utils/stats.h"

#include <chrono>

namespace Envoy {
namespace Utils {
namespace {

// The time interval for envoy stats update.
const int kStatsUpdateIntervalInMs = 10000;

}  // namespace

MixerStatsObject::MixerStatsObject(Event::Dispatcher& dispatcher,
                                   MixerFilterStats& stats,
                                   ::google::protobuf::Duration update_interval,
                                   GetStatsFunc func)
    : stats_(stats), get_stats_func_(func) {
  stats_update_interval_ =
      update_interval.seconds() * 1000 + update_interval.nanos() / 1000000;
  if (stats_update_interval_ <= 0) {
    stats_update_interval_ = kStatsUpdateIntervalInMs;
  }
  memset(&old_stats_, 0, sizeof(old_stats_));

  if (get_stats_func_) {
    timer_ = dispatcher.createTimer([this]() { OnTimer(); });
    timer_->enableTimer(std::chrono::milliseconds(stats_update_interval_));
  }
}

void MixerStatsObject::OnTimer() {
  ::istio::mixerclient::Statistics new_stats;
  bool get_stats = get_stats_func_(&new_stats);
  if (get_stats) {
    CheckAndUpdateStats(new_stats);
  }
  timer_->enableTimer(std::chrono::milliseconds(stats_update_interval_));
}

#define CHECK_AND_UPDATE_STATS(NAME)                   \
  if (new_stats.NAME > old_stats_.NAME) {              \
    stats_.NAME.add(new_stats.NAME - old_stats_.NAME); \
  }

void MixerStatsObject::CheckAndUpdateStats(
    const ::istio::mixerclient::Statistics& new_stats) {
  CHECK_AND_UPDATE_STATS(total_check_calls_);
  CHECK_AND_UPDATE_STATS(total_check_cache_hits_);
  CHECK_AND_UPDATE_STATS(total_check_cache_misses_);
  CHECK_AND_UPDATE_STATS(total_check_cache_hit_accepts_);
  CHECK_AND_UPDATE_STATS(total_check_cache_hit_denies_);
  CHECK_AND_UPDATE_STATS(total_remote_check_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_check_accepts_);
  CHECK_AND_UPDATE_STATS(total_remote_check_denies_);
  CHECK_AND_UPDATE_STATS(total_quota_calls_);
  CHECK_AND_UPDATE_STATS(total_quota_cache_hits_);
  CHECK_AND_UPDATE_STATS(total_quota_cache_misses_);
  CHECK_AND_UPDATE_STATS(total_quota_cache_hit_accepts_);
  CHECK_AND_UPDATE_STATS(total_quota_cache_hit_denies_);
  CHECK_AND_UPDATE_STATS(total_remote_quota_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_quota_accepts_);
  CHECK_AND_UPDATE_STATS(total_remote_quota_denies_);
  CHECK_AND_UPDATE_STATS(total_remote_quota_prefetch_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_call_successes_);
  CHECK_AND_UPDATE_STATS(total_remote_call_timeouts_);
  CHECK_AND_UPDATE_STATS(total_remote_call_send_errors_);
  CHECK_AND_UPDATE_STATS(total_remote_call_other_errors_);
  CHECK_AND_UPDATE_STATS(total_remote_call_retries_);
  CHECK_AND_UPDATE_STATS(total_remote_call_cancellations_);

  CHECK_AND_UPDATE_STATS(total_report_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_report_calls_);
  CHECK_AND_UPDATE_STATS(total_remote_report_successes_);
  CHECK_AND_UPDATE_STATS(total_remote_report_timeouts_);
  CHECK_AND_UPDATE_STATS(total_remote_report_send_errors_);
  CHECK_AND_UPDATE_STATS(total_remote_report_other_errors_);

  // Copy new_stats to old_stats_ for next stats update.
  old_stats_ = new_stats;
}

}  // namespace Utils
}  // namespace Envoy
