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

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/istio/mixerclient/check_response.h"
#include "include/istio/mixerclient/client.h"
#include "include/istio/utils/attributes_builder.h"
#include "src/istio/mixerclient/status_test_util.h"
#include "src/istio/utils/logger.h"

using ::google::protobuf::util::Status;
using ::google::protobuf::util::error::Code;
using ::istio::mixer::v1::Attributes;
using ::istio::mixer::v1::CheckRequest;
using ::istio::mixer::v1::CheckResponse;
using ::istio::mixerclient::CheckContextSharedPtr;
using ::istio::mixerclient::CheckResponseInfo;
using ::istio::quota_config::Requirement;
using ::testing::_;
using ::testing::Invoke;

namespace istio {
namespace mixerclient {
namespace {

const std::string kRequestCount = "RequestCount";

// A mocking class to mock CheckTransport interface.
class MockCheckTransport {
 public:
  MOCK_METHOD3(Check, void(const CheckRequest&, CheckResponse*, DoneFunc));
  TransportCheckFunc GetFunc() {
    return [this](const CheckRequest& request, CheckResponse* response,
                  DoneFunc on_done) -> CancelFunc {
      Check(request, response, on_done);
      return nullptr;
    };
  }
};

class MixerClientImplTest : public ::testing::Test {
 public:
  MixerClientImplTest() {
    CreateClient(true /* check_cache */, true /* quota_cache */);
  }

 protected:
  void CreateClient(bool check_cache, bool quota_cache) {
    MixerClientOptions options(CheckOptions(check_cache ? 1 : 0 /*entries */),
                               ReportOptions(1, 1000),
                               QuotaOptions(quota_cache ? 1 : 0 /* entries */,
                                            600000 /* expiration_ms */));
    options.check_options.network_fail_open = false;
    options.env.check_transport = mock_check_transport_.GetFunc();
    client_ = CreateMixerClient(options);
  }

  void CheckStatisticsInvariants(const Statistics& stats) {
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

    EXPECT_EQ(stats.total_check_calls_,
              stats.total_check_cache_hits_ + stats.total_check_cache_misses_);
    EXPECT_EQ(stats.total_check_cache_hits_,
              stats.total_check_cache_hit_accepts_ +
                  stats.total_check_cache_hit_denies_);
    EXPECT_EQ(stats.total_remote_check_calls_, stats.total_check_cache_misses_);
    EXPECT_GE(
        stats.total_remote_check_calls_,
        stats.total_remote_check_accepts_ + stats.total_remote_check_denies_);

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

    EXPECT_EQ(stats.total_quota_calls_,
              stats.total_quota_cache_hits_ + stats.total_quota_cache_misses_);
    EXPECT_EQ(stats.total_quota_cache_hits_,
              stats.total_quota_cache_hit_accepts_ +
                  stats.total_quota_cache_hit_denies_);
    EXPECT_EQ(stats.total_remote_quota_calls_,
              stats.total_quota_cache_misses_ +
                  stats.total_remote_quota_prefetch_calls_);
    EXPECT_GE(
        stats.total_remote_quota_calls_,
        stats.total_remote_quota_accepts_ + stats.total_remote_quota_denies_);

    //
    // Counters for upstream requests to Mixer.
    //
    // total_remote_calls = SUM(total_remote_call_successes, ...,
    // total_remote_call_other_errors) Total transport errors would be
    // (total_remote_calls - total_remote_call_successes).
    //

    EXPECT_EQ(stats.total_remote_calls_,
              stats.total_remote_call_successes_ +
                  stats.total_remote_call_timeouts_ +
                  stats.total_remote_call_send_errors_ +
                  stats.total_remote_call_other_errors_);
  }

  CheckContextSharedPtr CreateContext(int quota_request) {
    uint32_t retries{0};
    bool fail_open{false};
    istio::mixerclient::SharedAttributesSharedPtr attributes{
        new SharedAttributes()};
    istio::mixerclient::CheckContextSharedPtr context{
        new CheckContext(retries, fail_open, attributes)};
    if (0 < quota_request) {
      context->quotaRequirements().push_back({kRequestCount, quota_request});
    }
    return context;
  }

  std::unique_ptr<MixerClient> client_;
  MockCheckTransport mock_check_transport_;
  TransportCheckFunc empty_transport_;
};

TEST_F(MixerClientImplTest, TestSuccessCheck) {
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke([](const CheckRequest& request, CheckResponse* response,
                          DoneFunc on_done) {
        response->mutable_precondition()->set_valid_use_count(1000);
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(0);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  for (size_t i = 0; i < 10; i++) {
    CheckContextSharedPtr context = CreateContext(0);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 11);
  // The first check call misses the policy cache, the rest hit and are accepted
  EXPECT_EQ(stat.total_check_cache_hits_, 10);
  EXPECT_EQ(stat.total_check_cache_misses_, 1);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 10);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_check_calls_, 1);
  EXPECT_EQ(stat.total_remote_check_accepts_, 1);
  EXPECT_EQ(stat.total_remote_check_denies_, 0);
  // Empty quota does not trigger any quota call.
  EXPECT_EQ(stat.total_quota_calls_, 0);
  EXPECT_EQ(stat.total_quota_cache_hits_, 0);
  EXPECT_EQ(stat.total_quota_cache_misses_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 0);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 0);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, 0);
  // Only one remote call and it succeeds
  EXPECT_EQ(stat.total_remote_calls_, 1);
  EXPECT_EQ(stat.total_remote_call_successes_, 1);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestPerRequestTransport) {
  // Global transport should not be called.
  EXPECT_CALL(mock_check_transport_, Check(_, _, _)).Times(0);

  // For local pre-request transport.
  MockCheckTransport local_check_transport;
  EXPECT_CALL(local_check_transport, Check(_, _, _))
      .WillOnce(Invoke([](const CheckRequest& request, CheckResponse* response,
                          DoneFunc on_done) {
        response->mutable_precondition()->set_valid_use_count(1000);
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(0);
    Status status;
    client_->Check(
        context, local_check_transport.GetFunc(),
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  for (size_t i = 0; i < 10; i++) {
    CheckContextSharedPtr context = CreateContext(0);
    Status status;
    client_->Check(
        context, local_check_transport.GetFunc(),
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 11);
  // The first check call misses the policy cache, the rest hit and are accepted
  EXPECT_EQ(stat.total_check_cache_hits_, 10);
  EXPECT_EQ(stat.total_check_cache_misses_, 1);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 10);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_check_calls_, 1);
  EXPECT_EQ(stat.total_remote_check_accepts_, 1);
  EXPECT_EQ(stat.total_remote_check_denies_, 0);
  // Empty quota does not trigger any quota call.
  EXPECT_EQ(stat.total_quota_calls_, 0);
  EXPECT_EQ(stat.total_quota_cache_hits_, 0);
  EXPECT_EQ(stat.total_quota_cache_misses_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 0);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 0);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, 0);
  // Only one remote call and it succeeds
  EXPECT_EQ(stat.total_remote_calls_, 1);
  EXPECT_EQ(stat.total_remote_call_successes_, 1);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestNoCheckCache) {
  CreateClient(false /* check_cache */, true /* quota_cache */);

  int call_counts = 0;
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillRepeatedly(Invoke([&](const CheckRequest& request,
                                 CheckResponse* response, DoneFunc on_done) {
        response->mutable_precondition()->set_valid_use_count(1000);
        CheckResponse::QuotaResult quota_result;
        quota_result.set_granted_amount(10);
        quota_result.mutable_valid_duration()->set_seconds(10);
        (*response->mutable_quotas())[kRequestCount] = quota_result;
        call_counts++;
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  for (size_t i = 0; i < 10; i++) {
    // Other calls are not cached.
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }
  // Call count 11 since check is not cached.
  EXPECT_EQ(call_counts, 11);
  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 11);
  EXPECT_EQ(stat.total_check_cache_hits_, 0);
  EXPECT_EQ(stat.total_check_cache_misses_, 11);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_check_calls_, 11);
  EXPECT_EQ(stat.total_remote_check_accepts_, 11);
  EXPECT_EQ(stat.total_remote_check_denies_, 0);
  //
  // The current quota cache impl forces a cache miss whenever the check cache
  // is missed.
  //
  EXPECT_EQ(stat.total_quota_calls_, 11);
  EXPECT_EQ(stat.total_quota_cache_hits_, 0);
  EXPECT_EQ(stat.total_quota_cache_misses_, 11);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 11);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 11);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, 0);
  // And all remote quota calls succeed
  EXPECT_EQ(stat.total_remote_calls_, 11);
  EXPECT_EQ(stat.total_remote_call_successes_, 11);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestNoQuotaCache) {
  CreateClient(true /* check_cache */, false /* quota_cache */);

  int call_counts = 0;
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillRepeatedly(Invoke([&](const CheckRequest& request,
                                 CheckResponse* response, DoneFunc on_done) {
        auto request_quotas = request.quotas();
        auto requested_amount = request_quotas[kRequestCount].amount();
        response->mutable_precondition()->set_valid_use_count(1000);
        CheckResponse::QuotaResult quota_result;
        quota_result.set_granted_amount(requested_amount);
        quota_result.mutable_valid_duration()->set_seconds(10);
        (*response->mutable_quotas())[kRequestCount] = quota_result;
        call_counts++;
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  for (size_t i = 0; i < 10; i++) {
    // Other calls should be cached.
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }
  // Call count 11 since quota is not cached.
  EXPECT_EQ(call_counts, 11);
  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 11);
  // The first check call misses the policy cache, the rest hit and are accepted
  EXPECT_EQ(stat.total_check_cache_hits_, 10);
  EXPECT_EQ(stat.total_check_cache_misses_, 1);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 10);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_check_calls_, 1);
  EXPECT_EQ(stat.total_remote_check_accepts_, 1);
  EXPECT_EQ(stat.total_remote_check_denies_, 0);
  EXPECT_EQ(stat.total_quota_calls_, 11);
  EXPECT_EQ(stat.total_quota_cache_hits_, 0);
  EXPECT_EQ(stat.total_quota_cache_misses_, 11);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 11);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 11);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, 0);
  // And all remote quota calls succeed
  EXPECT_EQ(stat.total_remote_calls_, 11);
  EXPECT_EQ(stat.total_remote_call_successes_, 11);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestSuccessCheckAndQuota) {
  int call_counts = 0;
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillRepeatedly(Invoke([&](const CheckRequest& request,
                                 CheckResponse* response, DoneFunc on_done) {
        auto request_quotas = request.quotas();
        auto requested_amount = request_quotas[kRequestCount].amount();
        response->mutable_precondition()->set_valid_use_count(1000);
        CheckResponse::QuotaResult quota_result;
        quota_result.set_granted_amount(requested_amount);
        quota_result.mutable_valid_duration()->set_seconds(10);
        (*response->mutable_quotas())[kRequestCount] = quota_result;
        call_counts++;
        on_done(Status::OK);
      }));

  // quota cache starts with 1 resource.  by requesting exactly 1 the request
  // will be satisfied by the cache and a background request will be initiated
  // to store 2 more in the cache
  {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  // Half of the requests from now on will be satisfied by the cache but require
  // background refills.
  for (size_t i = 0; i < 100; i++) {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_TRUE(status.ok());
  }

  // The number of remote prefetch calls should be less than or equal to the
  // current prefetch impl's value of 6.   Decreases are of course acceptable,
  // but increases should be allowed only with a good reason.
  int expected_prefetchs = 6;

  EXPECT_EQ(call_counts, 1 + expected_prefetchs);
  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 101);
  // The first check call misses the policy cache, the rest hit and are accepted
  EXPECT_EQ(stat.total_check_cache_hits_, 100);
  EXPECT_EQ(stat.total_check_cache_misses_, 1);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 100);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_check_calls_, 1);
  EXPECT_EQ(stat.total_remote_check_accepts_, 1);
  EXPECT_EQ(stat.total_remote_check_denies_, 0);
  // Quota cache is always hit because of the quota prefetch mechanism.
  EXPECT_EQ(stat.total_quota_calls_, 101);
  EXPECT_EQ(stat.total_quota_cache_hits_, 100);
  EXPECT_EQ(stat.total_quota_cache_misses_, 1);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 100);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 1 + expected_prefetchs);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 1 + expected_prefetchs);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, expected_prefetchs);
  // And all remote quota calls succeed
  EXPECT_EQ(stat.total_remote_calls_, 1 + expected_prefetchs);
  EXPECT_EQ(stat.total_remote_call_successes_, 1 + expected_prefetchs);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestFailedCheckAndQuota) {
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke([](const CheckRequest& request, CheckResponse* response,
                          DoneFunc on_done) {
        response->mutable_precondition()->mutable_status()->set_code(
            Code::FAILED_PRECONDITION);
        response->mutable_precondition()->set_valid_use_count(100);
        CheckResponse::QuotaResult quota_result;
        quota_result.set_granted_amount(10);
        quota_result.mutable_valid_duration()->set_seconds(10);
        (*response->mutable_quotas())[kRequestCount] = quota_result;
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_ERROR_CODE(Code::FAILED_PRECONDITION, status);
  }

  for (size_t i = 0; i < 10; i++) {
    // Other calls should be cached.
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_ERROR_CODE(Code::FAILED_PRECONDITION, status);
  }
  Statistics stat;
  client_->GetStatistics(&stat);
  CheckStatisticsInvariants(stat);

  EXPECT_EQ(stat.total_check_calls_, 11);
  // The first call is a remote blocking call, which returns failed precondition
  // in check response. Following calls only make check cache calls and return.
  EXPECT_EQ(stat.total_check_cache_hits_, 10);
  EXPECT_EQ(stat.total_check_cache_misses_, 1);
  EXPECT_EQ(stat.total_check_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_check_cache_hit_denies_, 10);
  EXPECT_EQ(stat.total_remote_check_calls_, 1);
  EXPECT_EQ(stat.total_remote_check_accepts_, 0);
  EXPECT_EQ(stat.total_remote_check_denies_, 1);
  // If the check cache denies the request, the quota cache never sees it.
  EXPECT_EQ(stat.total_quota_calls_, 1);
  EXPECT_EQ(stat.total_quota_cache_hits_, 0);
  EXPECT_EQ(stat.total_quota_cache_misses_, 1);
  EXPECT_EQ(stat.total_quota_cache_hit_accepts_, 0);
  EXPECT_EQ(stat.total_quota_cache_hit_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_calls_, 1);
  EXPECT_EQ(stat.total_remote_quota_accepts_, 1);
  EXPECT_EQ(stat.total_remote_quota_denies_, 0);
  EXPECT_EQ(stat.total_remote_quota_prefetch_calls_, 0);
  // Only one remote call and it succeeds at the transport level
  EXPECT_EQ(stat.total_remote_calls_, 1);
  EXPECT_EQ(stat.total_remote_call_successes_, 1);
  EXPECT_EQ(stat.total_remote_call_timeouts_, 0);
  EXPECT_EQ(stat.total_remote_call_send_errors_, 0);
  EXPECT_EQ(stat.total_remote_call_other_errors_, 0);
}

TEST_F(MixerClientImplTest, TestUnavailableQuotaBackend) {
  EXPECT_CALL(mock_check_transport_, Check(_, _, _))
      .WillOnce(Invoke([](const CheckRequest& request, CheckResponse* response,
                          DoneFunc on_done) {
        response->mutable_precondition()->set_valid_use_count(100);
        CheckResponse::QuotaResult quota_result;
        quota_result.mutable_status()->set_code(Code::UNAVAILABLE);
        // explicitly do not set granted amounts.
        (*response->mutable_quotas())[kRequestCount] = quota_result;
        on_done(Status::OK);
      }));

  {
    CheckContextSharedPtr context = CreateContext(1);
    Status status;
    client_->Check(
        context, empty_transport_,
        [&status](const CheckResponseInfo& info) { status = info.status(); });
    EXPECT_ERROR_CODE(Code::OK, status);
  }
}

}  // namespace
}  // namespace mixerclient
}  // namespace istio
