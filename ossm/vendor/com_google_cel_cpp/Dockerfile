# This Dockerfile is used to create a container around gcc9 and bazel for
# building the CEL C++ library on GitHub.
#
# To update a new version of this container, use gcloud. You may need to run
# `gcloud auth login` and `gcloud auth configure-docker` first.
#
# Note, if you need to run docker using `sudo` use the following commands
# instead:
#
#     sudo gcloud auth login --no-launch-browser
#     sudo gcloud auth configure-docker
#
# Run the following command from the root of the CEL repository:
#
#     gcloud builds submit --region=us -t gcr.io/cel-analysis/gcc9 .
#
# Once complete get the sha256 digest from the output using the following
# command:
#
#     gcloud artifacts versions list --package=gcc9 --repository=gcr.io \
#       --location=us
#
# The cloudbuild.yaml file must be updated to use the new digest like so:
#
#     - name: 'gcr.io/cel-analysis/gcc9@<SHA256>'
FROM gcc:9

# Install Bazel prerequesites and required tools.
# See https://docs.bazel.build/versions/master/install-ubuntu.html
RUN apt-get update && \
    apt-get upgrade -y && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      git \
      libssl-dev \
      make \
      pkg-config \
      python3 \
      unzip \
      wget \
      zip \
      zlib1g-dev \
      default-jdk-headless \
      clang-11 && \
    apt-get clean

# Install Bazel.
# https://github.com/bazelbuild/bazel/releases
ARG BAZEL_VERSION="7.3.2"
ADD https://github.com/bazelbuild/bazel/releases/download/${BAZEL_VERSION}/bazel-${BAZEL_VERSION}-installer-linux-x86_64.sh /tmp/install_bazel.sh
RUN /bin/bash /tmp/install_bazel.sh && rm /tmp/install_bazel.sh

RUN mkdir -p /workspace
RUN mkdir -p /bazel

ENTRYPOINT ["/usr/local/bin/bazel"]
