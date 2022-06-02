// Copyright Istio Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "metadata_object.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace Envoy {
namespace Common {

class WorkloadMetadataObjectTest : public testing::Test {
 public:
  WorkloadMetadataObjectTest() { ENVOY_LOG_MISC(info, "test"); }
};

TEST_F(WorkloadMetadataObjectTest, Hash) {
  WorkloadMetadataObject obj1("foo-pod-12345", "default", "foo", "foo",
                              "latest", WorkloadType::KUBERNETES_DEPLOYMENT, {},
                              {});
  WorkloadMetadataObject obj2("foo-pod-12345", "default", "bar", "baz", "first",
                              WorkloadType::KUBERNETES_JOB, {}, {});

  EXPECT_EQ(obj1.hash().value(), obj2.hash().value());
}

TEST_F(WorkloadMetadataObjectTest, Baggage) {
  WorkloadMetadataObject deploy("pod-foo-1234", "default", "foo", "foo-service",
                                "v1alpha3", WorkloadType::KUBERNETES_DEPLOYMENT,
                                {"10.10.10.1", "192.168.1.1"},
                                {"app", "storage"});

  WorkloadMetadataObject pod("pod-foo-1234", "default", "foo", "foo-service",
                             "v1alpha3", WorkloadType::KUBERNETES_POD,
                             {"10.10.10.1", "192.168.1.1"}, {"app", "storage"});

  WorkloadMetadataObject cronjob(
      "pod-foo-1234", "default", "foo", "foo-service", "v1alpha3",
      WorkloadType::KUBERNETES_CRONJOB, {"10.10.10.1", "192.168.1.1"},
      {"app", "storage"});

  WorkloadMetadataObject job("pod-foo-1234", "default", "foo", "foo-service",
                             "v1alpha3", WorkloadType::KUBERNETES_JOB,
                             {"10.10.10.1", "192.168.1.1"}, {"app", "storage"});

  EXPECT_EQ(
      deploy.baggage(),
      absl::StrCat(
          "k8s.namespace.name=default,k8s.deployment.name=foo,service.name=",
          "foo-service,service.version=v1alpha3"));

  EXPECT_EQ(
      pod.baggage(),
      absl::StrCat("k8s.namespace.name=default,k8s.pod.name=foo,service.name=",
                   "foo-service,service.version=v1alpha3"));

  EXPECT_EQ(cronjob.baggage(),
            absl::StrCat(
                "k8s.namespace.name=default,k8s.cronjob.name=foo,service.name=",
                "foo-service,service.version=v1alpha3"));

  EXPECT_EQ(
      job.baggage(),
      absl::StrCat("k8s.namespace.name=default,k8s.job.name=foo,service.name=",
                   "foo-service,service.version=v1alpha3"));
}

}  // namespace Common
}  // namespace Envoy
