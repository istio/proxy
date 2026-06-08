FROM almalinux:9.6

ARG USER_ID=1000
ARG GROUP_ID=1000
ARG TARGETARCH

ENV BAZELISK_VERSION=v1.26.0
ENV BAZELISK_URL=https://github.com/bazelbuild/bazelisk/releases/download/${BAZELISK_VERSION}/bazelisk-linux-${TARGETARCH}

RUN dnf install -y \
    ca-certificates \
    gcc-c++ \
    rpm-build \
    && dnf clean all \
    && curl -sSL -o /usr/local/bin/bazel ${BAZELISK_URL} \
    && chmod +x /usr/local/bin/bazel

RUN groupadd --gid $GROUP_ID devgroup && \
    useradd --uid $USER_ID --gid $GROUP_ID --create-home --shell /bin/bash -l devuser

VOLUME /home/devuser/.cache

RUN mkdir -p /home/devuser/.cache && \
    chown -R ${USER_ID}:${GROUP_ID} /home/devuser/.cache

USER devuser
WORKDIR /home/devuser
