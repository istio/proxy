#include <sstream>

#include "platform_util.h"
#include "test.h"

using namespace datadog::tracing;

#define PLATFORM_UTIL_TEST(x) TEST_CASE(x, "[platform_util]")

PLATFORM_UTIL_TEST("find docker container ID") {
  struct TestCase {
    size_t line;
    std::string_view name;
    std::string input;
    Optional<std::string> expected_container_id;
  };

  auto test_case = GENERATE(values<TestCase>({
      {__LINE__, "empty inputs", "", nullopt},
      {__LINE__, "no docker container ID", "coucou", nullopt},
      {__LINE__, "one line with docker container ID",
       "0::/system.slice/"
       "docker-"
       "cde7c2bab394630a42d73dc610b9c57415dced996106665d427f6d0566594411.scope",
       "cde7c2bab394630a42d73dc610b9c57415dced996106665d427f6d0566594411"},
      {__LINE__, "multiline wihtout docker container ID", R"(
0::/
10:memory:/user.slice/user-0.slice/session-14.scope
9:hugetlb:/
8:cpuset:/
7:pids:/user.slice/user-0.slice/session-14.scope
6:freezer:/
5:net_cls,net_prio:/
4:perf_event:/
3:cpu,cpuacct:/user.slice/user-0.slice/session-14.scope
2:devices:/user.slice/user-0.slice/session-14.scope
1:name=systemd:/user.slice/user-0.slice/session-14.scope
)",
       nullopt},
      {__LINE__, "multiline with docker container ID", R"(
11:blkio:/user.slice/user-0.slice/session-14.scope
10:memory:/user.slice/user-0.slice/session-14.scope
9:hugetlb:/
8:cpuset:/
7:pids:/user.slice/user-0.slice/session-14.scope
3:cpu:/system.slice/docker-cde7c2bab394630a42d73dc610b9c57415dced996106665d427f6d0566594411.scope
6:freezer:/
5:net_cls,net_prio:/
4:perf_event:/
3:cpu,cpuacct:/user.slice/user-0.slice/session-14.scope
2:devices:/user.slice/user-0.slice/session-14.scope
1:name=systemd:/user.slice/user-0.slice/session-14.scope
    )",
       "cde7c2bab394630a42d73dc610b9c57415dced996106665d427f6d0566594411"},
  }));

  CAPTURE(test_case.name);

  std::istringstream is(test_case.input);

  auto maybe_container_id = container::find_container_id(is);
  if (test_case.expected_container_id.has_value()) {
    REQUIRE(maybe_container_id.has_value());
    CHECK(*maybe_container_id == *test_case.expected_container_id);
  } else {
    CHECK(!maybe_container_id.has_value());
  }
}

PLATFORM_UTIL_TEST("find multiline container IDs") {
  struct TestCase {
    size_t line;
    std::string_view name;
    std::string input;
    Optional<std::string> expected_container_id;
  };

  auto test_case = GENERATE(values<TestCase>({
      {__LINE__, "Docker", R"(
13:name=systemd:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
12:pids:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
11:hugetlb:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
10:net_prio:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
9:perf_event:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
8:net_cls:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
7:freezer:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
6:devices:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
5:memory:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
4:blkio:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
3:cpuacct:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
2:cpu:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
1:cpuset:/docker/3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860
    )",
       "3726184226f5d3147c25fdeab5b60097e378e8a720503a5e19ecfdf29f869860"},
      {__LINE__, "Kubernetes", R"(
11:perf_event:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
10:pids:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
9:memory:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
8:cpu,cpuacct:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
7:blkio:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
6:cpuset:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
5:devices:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
4:freezer:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
3:net_cls,net_prio:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
2:hugetlb:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
1:name=systemd:/kubepods/besteffort/pod3d274242-8ee0-11e9-a8a6-1e68d864ef1a/3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1
1:name=systemd:/kubepods.slice/kubepods-burstable.slice/kubepods-burstable-pod2d3da189_6407_48e3_9ab6_78188d75e609.slice/docker-3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1.scope
    )",
       "3e74d3fd9db4c9dd921ae05c2502fb984d0cde1b36e581b13f79c639da4518a1"},
      {__LINE__, "Ecs", R"(
9:perf_event:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
8:memory:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
7:hugetlb:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
6:freezer:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
5:devices:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
4:cpuset:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
3:cpuacct:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
2:cpu:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
1:blkio:/ecs/haissam-ecs-classic/5a0d5ceddf6c44c1928d367a815d890f/38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce
    )",
       "38fac3e99302b3622be089dd41e7ccf38aff368a86cc339972075136ee2710ce"},
      {__LINE__, "Fargate1Dot3", R"(
11:hugetlb:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
10:pids:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
9:cpuset:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
8:net_cls,net_prio:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
7:cpu,cpuacct:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
6:perf_event:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
5:freezer:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
4:devices:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
3:blkio:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
2:memory:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
1:name=systemd:/ecs/55091c13-b8cf-4801-b527-f4601742204d/432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da
    )",
       "432624d2150b349fe35ba397284dea788c2bf66b885d14dfc1569b01890ca7da"},
      {__LINE__, "Fargate1Dot4", R"(
11:hugetlb:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
10:pids:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
9:cpuset:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
8:net_cls,net_prio:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
7:cpu,cpuacct:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
6:perf_event:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
5:freezer:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
4:devices:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
3:blkio:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
2:memory:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
1:name=systemd:/ecs/34dc0b5e626f2c5c4c5170e34b10e765-1234567890
    )",
       "34dc0b5e626f2c5c4c5170e34b10e765-1234567890"},
      {__LINE__, "EksNodegroup", R"(
11:blkio:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
10:cpuset:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
9:perf_event:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
8:memory:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
7:pids:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
6:cpu,cpuacct:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
5:net_cls,net_prio:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
4:devices:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
3:freezer:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
2:hugetlb:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
1:name=systemd:/kubepods.slice/kubepods-pod9508fe66_7675_4003_b7c9_d83e9f8f85e5.slice/cri-containerd-26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4.scope
    )",
       "26cfbe35e08b24f053011af4ada23d8fcbf81f27f8331a94f56de5b677c903e4"},
      {__LINE__, "PcfContainer1", R"(
12:memory:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
11:rdma:/
10:freezer:/garden/6f265890-5165-7fab-6b52-18d1
9:hugetlb:/garden/6f265890-5165-7fab-6b52-18d1
8:pids:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
7:perf_event:/garden/6f265890-5165-7fab-6b52-18d1
6:cpu,cpuacct:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
5:net_cls,net_prio:/garden/6f265890-5165-7fab-6b52-18d1
4:cpuset:/garden/6f265890-5165-7fab-6b52-18d1
3:blkio:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
2:devices:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
1:name=systemd:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
    )",
       "6f265890-5165-7fab-6b52-18d1"},
      {__LINE__, "PcfContainer2", R"(
1:name=systemd:/system.slice/garden.service/garden/6f265890-5165-7fab-6b52-18d1
    )",
       "6f265890-5165-7fab-6b52-18d1"},
  }));

  CAPTURE(test_case.name);

  std::istringstream is(test_case.input);

  auto maybe_container_id = container::find_container_id(is);
  if (test_case.expected_container_id.has_value()) {
    REQUIRE(maybe_container_id.has_value());
    CHECK(*maybe_container_id == *test_case.expected_container_id);
  } else {
    CHECK(!maybe_container_id.has_value());
  }
}
