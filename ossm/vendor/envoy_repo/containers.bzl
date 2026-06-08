

REPO = "docker.io/envoyproxy/envoy-build"
REPO_GCR = "gcr.io/envoy-ci/envoy-build"
SHA = "20656853fae51927cda557e7af80ccff175f5de6f84bd0f092cd8672b2a6e0fe"
SHA_GCC = "439e870260c1599646d05b8b5d3bf1b6dd585c2e3cdac78dcb9f4081564c27fd"
SHA_MOBILE = "bd1338a8951376211e4f4f6ff3171675670c4c582b0966f1d247abd3ba6a8a67"
SHA_WORKER = "25a68eff24b7414a346977d545687b87851d1c5746c466798050fa12fc5d0686"
TAG = "86873047235e9b8232df989a5999b9bebf9db69c"

def image_gcc():
    return "%s@sha256:%s" % (
        REPO_GCR, SHA_GCC)

def image_mobile():
    return "%s@sha256:%s" % (
        REPO, SHA_MOBILE)

def image_worker():
    return "%s@sha256:%s" % (
        REPO_GCR, SHA_WORKER)

