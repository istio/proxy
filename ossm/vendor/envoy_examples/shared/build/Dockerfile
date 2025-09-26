FROM envoyproxy/envoy-build:ci-cdfbb76bc18c115270127132c6f7c67000cd2e74@sha256:48a8b1bc24ed771560126642dcbba33c50c6b837722ba9c75270b858ef7ab3a6
ENV DEBIAN_FRONTEND=noninteractive
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get -qq update \
    && apt-get -qq install --no-install-recommends -y gosu
