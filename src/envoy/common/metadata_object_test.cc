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
#include "test/mocks/ssl/mocks.h"

namespace Envoy {
namespace Common {

class WorkloadMetadataObjectTest : public testing::Test {
 public:
  WorkloadMetadataObjectTest() { ENVOY_LOG_MISC(info, "test"); }
};

TEST_F(WorkloadMetadataObjectTest, Hash) {
  WorkloadMetadataObject obj1("foo-pod-12345", "my-cluster", "default", "foo",
                              "foo", "latest",
                              WorkloadType::KUBERNETES_DEPLOYMENT, {}, {});
  WorkloadMetadataObject obj2("foo-pod-12345", "my-cluster", "default", "bar",
                              "baz", "first", WorkloadType::KUBERNETES_JOB, {},
                              {});

  EXPECT_EQ(obj1.hash().value(), obj2.hash().value());
}

TEST_F(WorkloadMetadataObjectTest, Baggage) {
  WorkloadMetadataObject deploy(
      "pod-foo-1234", "my-cluster", "default", "foo", "foo-service", "v1alpha3",
      WorkloadType::KUBERNETES_DEPLOYMENT, {"10.10.10.1", "192.168.1.1"},
      {"app", "storage"});

  WorkloadMetadataObject pod("pod-foo-1234", "my-cluster", "default", "foo",
                             "foo-service", "v1alpha3",
                             WorkloadType::KUBERNETES_POD,
                             {"10.10.10.1", "192.168.1.1"}, {"app", "storage"});

  WorkloadMetadataObject cronjob(
      "pod-foo-1234", "my-cluster", "default", "foo", "foo-service", "v1alpha3",
      WorkloadType::KUBERNETES_CRONJOB, {"10.10.10.1", "192.168.1.1"},
      {"app", "storage"});

  WorkloadMetadataObject job("pod-foo-1234", "my-cluster", "default", "foo",
                             "foo-service", "v1alpha3",
                             WorkloadType::KUBERNETES_JOB,
                             {"10.10.10.1", "192.168.1.1"}, {"app", "storage"});

  EXPECT_EQ(deploy.baggage(),
            absl::StrCat("k8s.cluster.name=my-cluster,",
                         "k8s.namespace.name=default,k8s.deployment.name=foo,",
                         "service.name=foo-service,service.version=v1alpha3"));

  EXPECT_EQ(pod.baggage(),
            absl::StrCat("k8s.cluster.name=my-cluster,",
                         "k8s.namespace.name=default,k8s.pod.name=foo,",
                         "service.name=foo-service,service.version=v1alpha3"));

  EXPECT_EQ(cronjob.baggage(),
            absl::StrCat("k8s.cluster.name=my-cluster,",
                         "k8s.namespace.name=default,k8s.cronjob.name=foo,"
                         "service.name=foo-service,service.version=v1alpha3"));

  EXPECT_EQ(job.baggage(),
            absl::StrCat("k8s.cluster.name=my-cluster,",
                         "k8s.namespace.name=default,k8s.job.name=foo,",
                         "service.name=foo-service,service.version=v1alpha3"));
}

using ::testing::NiceMock;

TEST_F(WorkloadMetadataObjectTest, FromBaggage) {
  const std::string ver = "v1.2";
  auto connection_info = std::make_shared<NiceMock<Ssl::MockConnectionInfo>>();
  ON_CALL(*connection_info, tlsVersion())
      .WillByDefault(testing::ReturnRef(ver));

  auto gotDeploy = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.cluster.name=my-cluster,k8s.namespace.name=default,",
                   "k8s.deployment.name=foo,service.name=foo-service,",
                   "service.version=v1alpha3"));

  EXPECT_EQ(gotDeploy->canonicalName(), "foo-service");
  EXPECT_EQ(gotDeploy->canonicalRevision(), "v1alpha3");
  EXPECT_EQ(gotDeploy->workloadType(), WorkloadType::KUBERNETES_DEPLOYMENT);
  EXPECT_EQ(gotDeploy->workloadName(), "foo");
  EXPECT_EQ(gotDeploy->namespaceName(), "default");
  EXPECT_EQ(gotDeploy->clusterName(), "my-cluster");
  EXPECT_EQ(gotDeploy->ssl(), nullptr);

  auto gotPod = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.cluster.name=my-cluster,k8s.namespace.name=test,k8s."
                   "pod.name=foo-pod-435,service.name=",
                   "foo-service,service.version=v1beta2"));

  EXPECT_EQ(gotPod->canonicalName(), "foo-service");
  EXPECT_EQ(gotPod->canonicalRevision(), "v1beta2");
  EXPECT_EQ(gotPod->workloadType(), WorkloadType::KUBERNETES_POD);
  EXPECT_EQ(gotPod->workloadName(), "foo-pod-435");
  EXPECT_EQ(gotPod->instanceName(), "foo-pod-435");
  EXPECT_EQ(gotPod->namespaceName(), "test");
  EXPECT_EQ(gotPod->clusterName(), "my-cluster");
  EXPECT_EQ(gotPod->ssl(), nullptr);

  auto gotJob = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.cluster.name=my-cluster,k8s.namespace.name=test,",
                   "k8s.job.name=foo-job-435,service.name=foo-service,",
                   "service.version=v1beta4"));

  EXPECT_EQ(gotJob->canonicalName(), "foo-service");
  EXPECT_EQ(gotJob->canonicalRevision(), "v1beta4");
  EXPECT_EQ(gotJob->workloadType(), WorkloadType::KUBERNETES_JOB);
  EXPECT_EQ(gotJob->workloadName(), "foo-job-435");
  EXPECT_EQ(gotJob->instanceName(), "foo-job-435");
  EXPECT_EQ(gotJob->namespaceName(), "test");
  EXPECT_EQ(gotJob->clusterName(), "my-cluster");
  EXPECT_EQ(gotJob->ssl(), nullptr);

  auto gotCron = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.cluster.name=my-cluster,k8s.namespace.name=test,",
                   "k8s.cronjob.name=foo-cronjob,service.name=foo-service,",
                   "service.version=v1beta4"));

  EXPECT_EQ(gotCron->canonicalName(), "foo-service");
  EXPECT_EQ(gotCron->canonicalRevision(), "v1beta4");
  EXPECT_EQ(gotCron->workloadType(), WorkloadType::KUBERNETES_CRONJOB);
  EXPECT_EQ(gotCron->workloadName(), "foo-cronjob");
  EXPECT_EQ(gotCron->namespaceName(), "test");
  EXPECT_EQ(gotCron->clusterName(), "my-cluster");
  EXPECT_EQ(gotCron->ssl(), nullptr);

  auto gotDeployWithSsl = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.cluster.name=my-cluster,k8s.namespace.name=default,",
                   "k8s.deployment.name=foo,service.name=foo-service,",
                   "service.version=v1alpha3"),
      connection_info);

  EXPECT_EQ(gotDeployWithSsl->canonicalName(), "foo-service");
  EXPECT_EQ(gotDeployWithSsl->canonicalRevision(), "v1alpha3");
  EXPECT_EQ(gotDeployWithSsl->workloadType(),
            WorkloadType::KUBERNETES_DEPLOYMENT);
  EXPECT_EQ(gotDeployWithSsl->workloadName(), "foo");
  EXPECT_EQ(gotDeployWithSsl->namespaceName(), "default");
  EXPECT_EQ(gotDeployWithSsl->clusterName(), "my-cluster");
  EXPECT_EQ(gotDeployWithSsl->ssl()->tlsVersion(), ver);

  auto gotNoCluster = WorkloadMetadataObject::fromBaggage(
      absl::StrCat("k8s.namespace.name=default,",
                   "k8s.deployment.name=foo,service.name=foo-service,",
                   "service.version=v1alpha3"));

  EXPECT_EQ(gotNoCluster->canonicalName(), "foo-service");
  EXPECT_EQ(gotNoCluster->canonicalRevision(), "v1alpha3");
  EXPECT_EQ(gotNoCluster->workloadType(), WorkloadType::KUBERNETES_DEPLOYMENT);
  EXPECT_EQ(gotNoCluster->workloadName(), "foo");
  EXPECT_EQ(gotNoCluster->namespaceName(), "default");
  EXPECT_EQ(gotNoCluster->clusterName(), "");
  EXPECT_EQ(gotNoCluster->ssl(), nullptr);
}

}  // namespace Common
}  // namespace Envoy
