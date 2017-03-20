
load("@git_istio_mixer_bzl//:repositories.bzl", "go_mixer_repositories")
load("@io_bazel_rules_go//go:def.bzl", "go_repository")

def test_repositories():
    go_mixer_repositories()
    go_repository(
        name = "com_github_istio_mixer",
        commit = "064001053b51f73adc3a80ff87ef41a15316c300",
        importpath = "github.com/istio/mixer",
    )
    
