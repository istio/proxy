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

#include "src/istio/mixerclient/report_batch.h"

#include "include/istio/utils/protobuf.h"
#include "src/istio/mixerclient/status_util.h"
#include "src/istio/utils/logger.h"

using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::ReportRequest;
using ::istio::mixer::v1::ReportResponse;

namespace istio {
namespace mixerclient {

static std::atomic<uint32_t> REPORT_FAIL_LOG_MESSAGES{0};
static constexpr uint32_t REPORT_FAIL_LOG_MODULUS{100};

ReportBatch::ReportBatch(const ReportOptions& options,
                         TransportReportFunc transport,
                         TimerCreateFunc timer_create,
                         AttributeCompressor& compressor)
    : options_(options),
      transport_(transport),
      timer_create_(timer_create),
      compressor_(compressor),
      batch_compressor_(compressor.CreateBatchCompressor()),
      total_report_calls_(0),
      total_remote_report_calls_(0) {}

ReportBatch::~ReportBatch() {}

void ReportBatch::Report(
    const istio::mixerclient::SharedAttributesSharedPtr& attributes) {
  std::lock_guard<std::mutex> lock(mutex_);
  ++total_report_calls_;
  batch_compressor_->Add(*attributes->attributes());
  if (batch_compressor_->size() >= options_.max_batch_entries) {
    FlushWithLock();
  } else {
    if (batch_compressor_->size() == 1 && timer_create_) {
      if (!timer_) {
        timer_ = timer_create_([this]() { Flush(); });
      }
      timer_->Start(options_.max_batch_time_ms);
    }
  }
}

void ReportBatch::FlushWithLock() {
  if (batch_compressor_->size() == 0) {
    return;
  }

  if (timer_) {
    timer_->Stop();
  }

  ++total_remote_report_calls_;
  auto request = batch_compressor_->Finish();
  std::shared_ptr<ReportResponse> response{new ReportResponse()};

  // TODO(jblatt) I replaced a ReportResponse raw pointer with a shared
  // pointer so at least the memory will be freed if this lambda is deleted
  // without being called, but really this should be a unique_ptr that is
  // moved into the transport_ and then moved into the lambda if invoked.
  auto shared_this = shared_from_this();
  transport_(
      request, &*response, [this, shared_this, response](const Status& status) {
        //
        // Classify and track transport errors
        //

        TransportResult result = TransportStatus(status);

        switch (result) {
          case TransportResult::SUCCESS:
            ++total_remote_report_successes_;
            break;
          case TransportResult::RESPONSE_TIMEOUT:
            ++total_remote_report_timeouts_;
            break;
          case TransportResult::SEND_ERROR:
            ++total_remote_report_send_errors_;
            break;
          case TransportResult::OTHER:
            ++total_remote_report_other_errors_;
            break;
        }

        if (!status.ok()) {
          if (MIXER_WARN_ENABLED &&
              0 == REPORT_FAIL_LOG_MESSAGES++ % REPORT_FAIL_LOG_MODULUS) {
            MIXER_WARN("Mixer Report failed with: %s",
                       status.ToString().c_str());
          } else {
            MIXER_DEBUG("Mixer Report failed with: %s",
                        status.ToString().c_str());
          }
          if (utils::InvalidDictionaryStatus(status)) {
            compressor_.ShrinkGlobalDictionary();
          }
        }
      });

  batch_compressor_->Clear();
}

void ReportBatch::Flush() {
  std::lock_guard<std::mutex> lock(mutex_);
  FlushWithLock();
}

}  // namespace mixerclient
}  // namespace istio
