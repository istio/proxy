/* Copyright 2020 Istio Authors. All Rights Reserved.
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

#include "extensions/stackdriver/common/utils.h"

#include "extensions/stackdriver/common/constants.h"
#include "gmock/gmock.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/message_differencer.h"
#include "test/test_common/status_utility.h"
#include "gtest/gtest.h"

namespace Extensions {
namespace Stackdriver {
namespace Common {

using google::protobuf::util::MessageDifferencer;

TEST(UtilsTest, TestEnvoyGrpcInsecure) {
  GrpcService expected_envoy_grpc_service;
  std::string envoy_google_grpc_json = R"({
        "google_grpc": {
            "target_uri": "test"
        }
    })";
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage(envoy_google_grpc_json, &expected_envoy_grpc_service, options));

  StackdriverStubOption opt;
  opt.insecure_endpoint = "test";
  GrpcService envoy_grpc_service;
  buildEnvoyGrpcService(opt, &envoy_grpc_service);

  std::string diff;
  MessageDifferencer differ;
  differ.ReportDifferencesToString(&diff);
  if (!differ.Compare(expected_envoy_grpc_service, envoy_grpc_service)) {
    FAIL() << "unexpected envoy grpc service " << diff << "\n";
  }
}

TEST(UtilsTest, TestEnvoyGrpcSTS) {
  GrpcService expected_envoy_grpc_service;
  std::string envoy_google_grpc_json = R"({
        "google_grpc": {
            "target_uri": "secure",
            "channel_credentials": {
                "ssl_credentials": {}
            },
            "call_credentials": {
                "sts_service": {
                    "token_exchange_service_uri": "http://localhost:1234/token",
                    "subject_token_path": "/var/run/secrets/tokens/istio-token",
                    "subject_token_type": "urn:ietf:params:oauth:token-type:jwt",
                    "scope": "https://www.googleapis.com/auth/cloud-platform"
                }
            }
        },
        "initial_metadata": {
            "key": "x-goog-user-project", 
            "value": "project"
        }
    })";
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage(envoy_google_grpc_json, &expected_envoy_grpc_service, options));

  StackdriverStubOption opt;
  opt.secure_endpoint = "secure";
  opt.sts_port = "1234";
  opt.project_id = "project";
  GrpcService envoy_grpc_service;
  buildEnvoyGrpcService(opt, &envoy_grpc_service);

  std::string diff;
  MessageDifferencer differ;
  differ.ReportDifferencesToString(&diff);
  if (!differ.Compare(expected_envoy_grpc_service, envoy_grpc_service)) {
    FAIL() << "unexpected envoy grpc service " << diff << "\n";
  }
}

TEST(UtilsTest, TestEnvoyGrpcDefaultCredential) {
  GrpcService expected_envoy_grpc_service;
  std::string envoy_google_grpc_json = R"({
        "google_grpc": {
            "target_uri": "secure",
            "channel_credentials": {
                "google_default": {}
            }
        }
    })";
  google::protobuf::util::JsonParseOptions options;
  ASSERT_OK(JsonStringToMessage(envoy_google_grpc_json, &expected_envoy_grpc_service, options));

  StackdriverStubOption opt;
  opt.secure_endpoint = "secure";
  GrpcService envoy_grpc_service;
  buildEnvoyGrpcService(opt, &envoy_grpc_service);

  std::string diff;
  MessageDifferencer differ;
  differ.ReportDifferencesToString(&diff);
  if (!differ.Compare(expected_envoy_grpc_service, envoy_grpc_service)) {
    FAIL() << "unexpected envoy grpc service " << diff << "\n";
  }
}

} // namespace Common
} // namespace Stackdriver
} // namespace Extensions
