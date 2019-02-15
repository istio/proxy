/* Copyright 2019 Istio Authors. All Rights Reserved.
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

#include "google/protobuf/arena.h"
#include "google/protobuf/stubs/status.h"
#include "include/istio/mixerclient/check_response.h"
#include "include/istio/quota_config/requirement.h"
#include "include/istio/utils/attribute_names.h"
#include "include/istio/utils/attributes_builder.h"
#include "mixer/v1/attributes.pb.h"
#include "mixer/v1/mixer.pb.h"
#include "src/istio/mixerclient/attribute_compressor.h"
#include "src/istio/mixerclient/check_cache.h"
#include "src/istio/mixerclient/quota_cache.h"
#include "src/istio/mixerclient/shared_attributes.h"

#include <vector>

namespace istio {
namespace mixerclient {

/**
 * All memory for the upstream policy and quota checks should hang off of these
 * objects.
 */
class CheckContext : public CheckResponseInfo {
 public:
  CheckContext(bool fail_open, SharedAttributesSharedPtr& shared_attributes)
      : shared_attributes_(shared_attributes), fail_open_(fail_open) {}

  const istio::mixer::v1::Attributes* attributes() const {
    return shared_attributes_->attributes();
  }

  const std::vector<istio::quota_config::Requirement>& quotaRequirements()
      const {
    return quota_requirements_;
  }
  std::vector<istio::quota_config::Requirement>& quotaRequirements() {
    return quota_requirements_;
  }

  //
  // Policy Cache Checks
  //

  bool policyCacheHit() const { return policy_cache_hit_; }
  const google::protobuf::util::Status& policyStatus() const {
    return policy_cache_result_.status();
  }

  void checkPolicyCache(CheckCache& policyCache) {
    policyCache.Check(*shared_attributes_->attributes(), &policy_cache_result_);
    policy_cache_hit_ = policy_cache_result_.IsCacheHit();
  }

  void updatePolicyCache(const google::protobuf::util::Status& status,
                         const istio::mixer::v1::CheckResponse& response) {
    policy_cache_result_.SetResponse(status, *shared_attributes_->attributes(),
                                     response);
  }

  //
  // Quota Cache Checks
  //

  bool quotaCheckRequired() const { return !quota_requirements_.empty(); }
  bool remoteQuotaRequestRequired() const {
    return remote_quota_check_required_;
  }

  void checkQuotaCache(QuotaCache& quotaCache) {
    if (!quotaCheckRequired()) {
      return;
    }

    //
    // Quota is removed from the quota cache iff there is a policy cache hit. If
    // there is a policy cache miss, then a request has to be sent upstream
    // anyways, so the quota will be decremented on the upstream response.
    //
    quotaCache.Check(*shared_attributes_->attributes(), quota_requirements_,
                     policyCacheHit(), &quota_cache_result_);

    remote_quota_check_required_ =
        quota_cache_result_.BuildRequest(allocRequestOnce());

    quota_cache_hit_ = quota_cache_result_.IsCacheHit();
  }

  void updateQuotaCache(const google::protobuf::util::Status& status,
                        const istio::mixer::v1::CheckResponse& response) {
    quota_cache_result_.SetResponse(status, *shared_attributes_->attributes(),
                                    response);
  }

  bool quotaCacheHit() const { return quota_cache_hit_; }
  const google::protobuf::util::Status& quotaStatus() const {
    return quota_cache_result_.status();
  }

  //
  // Upstream request and response
  //

  void compressRequest(const AttributeCompressor& compressor,
                       const std::string& deduplication_id) {
    compressor.Compress(*shared_attributes_->attributes(),
                        allocRequestOnce()->mutable_attributes());
    request_->set_global_word_count(compressor.global_word_count());
    request_->set_deduplication_id(deduplication_id);
  }

  bool networkFailOpen() const { return fail_open_; }

  const istio::mixer::v1::CheckRequest& request() { return *request_; }

  istio::mixer::v1::CheckResponse* response() {
    if (!response_) {
      response_ = google::protobuf::Arena::CreateMessage<
          istio::mixer::v1::CheckResponse>(&shared_attributes_->arena());
    }
    return response_;
  }

  void setFinalStatus(const google::protobuf::util::Status& status,
                      bool add_report_attributes = true) {
    if (add_report_attributes) {
      utils::AttributesBuilder builder(shared_attributes_->attributes());
      builder.AddBool(utils::AttributeName::kCheckCacheHit, policy_cache_hit_);
      builder.AddBool(utils::AttributeName::kQuotaCacheHit, quota_cache_hit_);
    }

    final_status_ = status;
  }

  //
  // CheckResponseInfo (exposed to the top-level filter)
  //

  const google::protobuf::util::Status& status() const override {
    return final_status_;
  }

  const istio::mixer::v1::RouteDirective& routeDirective() const override {
    return policy_cache_result_.route_directive();
  }

 private:
  istio::mixer::v1::CheckRequest* allocRequestOnce() {
    if (!request_) {
      request_ = google::protobuf::Arena::CreateMessage<
          istio::mixer::v1::CheckRequest>(&shared_attributes_->arena());
    }

    return request_;
  }

  istio::mixerclient::SharedAttributesSharedPtr shared_attributes_;
  std::vector<istio::quota_config::Requirement> quota_requirements_;

  bool quota_cache_hit_{false};
  bool policy_cache_hit_{false};

  QuotaCache::CheckResult quota_cache_result_;
  CheckCache::CheckResult policy_cache_result_;

  istio::mixer::v1::CheckRequest* request_{nullptr};
  istio::mixer::v1::CheckResponse* response_{nullptr};

  bool fail_open_{false};
  bool remote_quota_check_required_{false};
  google::protobuf::util::Status final_status_{
      google::protobuf::util::Status::UNKNOWN};
};

typedef std::shared_ptr<CheckContext> CheckContextSharedPtr;

}  // namespace mixerclient
}  // namespace istio
