# Use the JDK image to avoid installing it again.
FROM circleci/openjdk:latest

# this will install the latest version of bazel - unfortunately it won't
# work, since they break backward compat on every single release.
# Proxy is currently requiring 0.11.
#RUN \
#    sudo sh -c 'echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" > /etc/apt/sources.list.d/bazel.list ' && \
#    curl https://storage.googleapis.com/bazel-apt/doc/apt-key.pub.gpg | sudo apt-key add -

# clang is used for TSAN and ASAN tests
RUN sudo sh -c 'curl http://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -'
RUN sudo sh -c 'echo "deb http://apt.llvm.org/stretch/ llvm-toolchain-stretch-8 main" > /etc/apt/sources.list.d/llvm.list'

RUN sudo apt-get update && \
    sudo apt-get -y install \
    wget software-properties-common make python python-pip pkg-config \
    zlib1g-dev bash-completion bc libtool automake zip time g++-6 gcc-6 \
    clang-8 clang-format-8 clang-tidy-8 lld-8 libc++-8-dev libc++abi-8-dev \
    rsync ninja-build

# Use Bazelisk launcher for Bazel (automatically downloads required version of Bazel).
RUN cd /tmp && \
    wget https://github.com/bazelbuild/bazelisk/releases/download/v1.0/bazelisk-linux-amd64 && \
    sudo chmod +x bazelisk-linux-amd64 && \
    sudo mv bazelisk-linux-amd64 /usr/local/bin/bazel

# Instead of "apt-get -y install  golang"
RUN cd /tmp && \
    wget https://redirector.gvt1.com/edgedl/go/go1.11.5.linux-amd64.tar.gz && \
    sudo rm -rf /usr/local/go && \
    sudo tar -C /usr/local -xzf go1.11.5.linux-amd64.tar.gz && \
    sudo chown -R circleci /usr/local/go && \
    sudo ln -s /usr/local/go/bin/go /usr/local/bin

# instead of "apt-get -y install cmake", pin cmake version to 3.8.0
RUN cd /tmp && \
    wget https://github.com/Kitware/CMake/releases/download/v3.8.0/cmake-3.8.0-Linux-x86_64.tar.gz && \
    sudo tar -C /usr/local/ -xzf cmake-3.8.0-Linux-x86_64.tar.gz && \
    sudo chown -R circleci /usr/local/cmake-3.8.0-Linux-x86_64 && \
    sudo ln -s /usr/local/cmake-3.8.0-Linux-x86_64/bin/cmake /usr/local/bin

RUN bazel version

# For circleci unit test integration, "go test -v 2>&1 | go-junit-report > report.xml"
RUN go get -u github.com/jstemmer/go-junit-report
