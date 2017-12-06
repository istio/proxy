# Use the JDK image to avoid installing it again.
FROM    circleci/openjdk:latest

# Based on https://github.com/GoogleCloudPlatform/cloud-builders/blob/master/bazel/Dockerfile
RUN \
    sudo sh -c 'echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" > /etc/apt/sources.list.d/bazel.list ' && \
    curl https://storage.googleapis.com/bazel-apt/doc/apt-key.pub.gpg | sudo apt-key add -

RUN sudo apt-get update && \
    sudo apt-get -y install bazel \
    wget software-properties-common make cmake python python-pip \
    bc libtool automake zip time g++-6 gcc-6 \
    rsync

# Instead of "apt-get -y install  golang"
RUN cd /tmp && \
    wget https://redirector.gvt1.com/edgedl/go/go1.9.2.linux-amd64.tar.gz && \
    sudo rm -rf /usr/local/go && \
    sudo tar -C /usr/local -xzf go1.9.2.linux-amd64.tar.gz && \
    sudo chown -R circleci /usr/local/go && \
    sudo ln -s /usr/local/go/bin/go /usr/local/bin

RUN bazel version

# For circleci unit test integration, "go test -v 2>&1 | go-junit-report > report.xml"
RUN go get -u github.com/jstemmer/go-junit-report
