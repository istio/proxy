// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/blind_sign_auth/cached_blind_sign_auth.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "quiche/blind_sign_auth/test_tools/mock_blind_sign_auth_interface.h"
#include "quiche/common/platform/api/quiche_test.h"
#include "quiche/common/test_tools/quiche_test_utils.h"

namespace quiche {
namespace test {
namespace {

using ::testing::_;
using ::testing::Unused;

class CachedBlindSignAuthTest : public QuicheTest {
 protected:
  void SetUp() override {
    cached_blind_sign_auth_ =
        std::make_unique<CachedBlindSignAuth>(&mock_blind_sign_auth_interface_);
  }

  void TearDown() override {
    fake_tokens_.clear();
    cached_blind_sign_auth_.reset();
  }

 public:
  std::vector<BlindSignToken> MakeFakeTokens(int num_tokens) {
    std::vector<BlindSignToken> fake_tokens;
    for (int i = 0; i < kBlindSignAuthRequestMaxTokens; i++) {
      fake_tokens.push_back(BlindSignToken{absl::StrCat("token:", i),
                                           absl::Now() + absl::Hours(1)});
    }
    return fake_tokens;
  }

  std::vector<BlindSignToken> MakeExpiredTokens(int num_tokens) {
    std::vector<BlindSignToken> fake_tokens;
    for (int i = 0; i < kBlindSignAuthRequestMaxTokens; i++) {
      fake_tokens.push_back(BlindSignToken{absl::StrCat("token:", i),
                                           absl::Now() - absl::Hours(1)});
    }
    return fake_tokens;
  }

  MockBlindSignAuthInterface mock_blind_sign_auth_interface_;
  std::unique_ptr<CachedBlindSignAuth> cached_blind_sign_auth_;
  std::optional<std::string> oauth_token_ = "oauth_token";
  std::vector<BlindSignToken> fake_tokens_;
};

TEST_F(CachedBlindSignAuthTest, TestGetTokensOneCallSuccessful) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(1)
      .WillOnce([this](Unused, int num_tokens, Unused, Unused,
                       SignedTokenCallback callback) {
        fake_tokens_ = MakeFakeTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = 5;
  absl::Notification done;
  SignedTokenCallback callback =
      [num_tokens, &done](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i));
        }
        done.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(callback));
  done.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensMultipleRemoteCallsSuccessful) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(2)
      .WillRepeatedly([this](Unused, int num_tokens, Unused, Unused,
                             SignedTokenCallback callback) {
        fake_tokens_ = MakeFakeTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens - 1;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [num_tokens, &first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i));
        }
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();

  absl::Notification second;
  SignedTokenCallback second_callback =
      [num_tokens, &second](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        EXPECT_EQ(tokens->at(0).token,
                  absl::StrCat("token:", kBlindSignAuthRequestMaxTokens - 1));
        for (int i = 1; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i - 1));
        }
        second.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(second_callback));
  second.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensSecondRequestFilledFromCache) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(1)
      .WillOnce([this](Unused, int num_tokens, Unused, Unused,
                       SignedTokenCallback callback) {
        fake_tokens_ = MakeFakeTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens / 2;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [num_tokens, &first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i));
        }
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();

  absl::Notification second;
  SignedTokenCallback second_callback =
      [num_tokens, &second](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token,
                    absl::StrCat("token:", i + num_tokens));
        }
        second.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(second_callback));
  second.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensThirdRequestRefillsCache) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(2)
      .WillRepeatedly([this](Unused, int num_tokens, Unused, Unused,
                             SignedTokenCallback callback) {
        fake_tokens_ = MakeFakeTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens / 2;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [num_tokens, &first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i));
        }
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();

  absl::Notification second;
  SignedTokenCallback second_callback =
      [num_tokens, &second](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(num_tokens, tokens->size());
        for (int i = 0; i < num_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token,
                    absl::StrCat("token:", i + num_tokens));
        }
        second.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(second_callback));
  second.WaitForNotification();

  absl::Notification third;
  int third_request_tokens = 10;
  SignedTokenCallback third_callback =
      [third_request_tokens,
       &third](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(third_request_tokens, tokens->size());
        for (int i = 0; i < third_request_tokens; i++) {
          EXPECT_EQ(tokens->at(i).token, absl::StrCat("token:", i));
        }
        third.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, third_request_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(third_callback));
  third.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensRequestTooLarge) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(0);

  int num_tokens = kBlindSignAuthRequestMaxTokens + 1;
  SignedTokenCallback callback =
      [](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInvalidArgument);
        EXPECT_THAT(
            tokens.status().message(),
            absl::StrFormat("Number of tokens requested exceeds maximum: %d",
                            kBlindSignAuthRequestMaxTokens));
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(callback));
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensRequestNegative) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(0);

  int num_tokens = -1;
  SignedTokenCallback callback =
      [num_tokens](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInvalidArgument);
        EXPECT_THAT(tokens.status().message(),
                    absl::StrFormat("Negative number of tokens requested: %d",
                                    num_tokens));
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(callback));
}

TEST_F(CachedBlindSignAuthTest, TestHandleGetTokensResponseErrorHandling) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(2)
      .WillOnce([](Unused, int num_tokens, Unused, Unused,
                   SignedTokenCallback callback) {
        std::move(callback)(absl::InternalError("AuthAndSign failed"));
      })
      .WillOnce([this](Unused, int num_tokens, Unused, Unused,
                       SignedTokenCallback callback) {
        fake_tokens_ = MakeFakeTokens(num_tokens);
        fake_tokens_.pop_back();
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [&first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(), absl::StatusCode::kInternal);
        EXPECT_THAT(tokens.status().message(), "AuthAndSign failed");
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();

  absl::Notification second;
  SignedTokenCallback second_callback =
      [&second](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(),
                    absl::StatusCode::kResourceExhausted);
        second.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(second_callback));
  second.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestGetTokensZeroTokensRequested) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(0);

  int num_tokens = 0;
  SignedTokenCallback callback =
      [](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        QUICHE_EXPECT_OK(tokens);
        EXPECT_EQ(tokens->size(), 0);
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(callback));
}

TEST_F(CachedBlindSignAuthTest, TestExpiredTokensArePruned) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(1)
      .WillOnce([this](Unused, int num_tokens, Unused, Unused,
                       SignedTokenCallback callback) {
        fake_tokens_ = MakeExpiredTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [&first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(),
                    absl::StatusCode::kResourceExhausted);
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();
}

TEST_F(CachedBlindSignAuthTest, TestClearCacheRemovesTokens) {
  EXPECT_CALL(mock_blind_sign_auth_interface_,
              GetTokens(oauth_token_, kBlindSignAuthRequestMaxTokens, _, _, _))
      .Times(2)
      .WillRepeatedly([this](Unused, int num_tokens, Unused, Unused,
                             SignedTokenCallback callback) {
        fake_tokens_ = MakeExpiredTokens(num_tokens);
        std::move(callback)(absl::MakeSpan(fake_tokens_));
      });

  int num_tokens = kBlindSignAuthRequestMaxTokens / 2;
  absl::Notification first;
  SignedTokenCallback first_callback =
      [&first](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(),
                    absl::StatusCode::kResourceExhausted);
        first.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(first_callback));
  first.WaitForNotification();

  cached_blind_sign_auth_->ClearCache();

  absl::Notification second;
  SignedTokenCallback second_callback =
      [&second](absl::StatusOr<absl::Span<BlindSignToken>> tokens) {
        EXPECT_THAT(tokens.status().code(),
                    absl::StatusCode::kResourceExhausted);
        second.Notify();
      };

  cached_blind_sign_auth_->GetTokens(
      oauth_token_, num_tokens, ProxyLayer::kProxyA,
      BlindSignAuthServiceType::kChromeIpBlinding, std::move(second_callback));
  second.WaitForNotification();
}

}  // namespace
}  // namespace test
}  // namespace quiche
