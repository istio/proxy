
load("@git_istio_mixer_bzl//:repositories.bzl", "go_mixer_repositories")
load("@io_bazel_rules_go//go:def.bzl", "go_repository")

def test_repositories():
    go_mixer_repositories()
    go_repository(
        name = "com_github_istio_mixer",
        commit = "0b2ef133ff6c912855cd059a8f98721ed51d0f93",
        importpath = "github.com/istio/mixer",
    )
    
