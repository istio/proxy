FROM    circleci/openjdk:latest

# Based on https://github.com/GoogleCloudPlatform/cloud-builders/blob/master/bazel/Dockerfile
RUN \
    sudo sh -c 'echo "deb [arch=amd64] http://storage.googleapis.com/bazel-apt stable jdk1.8" > /etc/apt/sources.list.d/bazel.list '

RUN \
    curl https://storage.googleapis.com/bazel-apt/doc/apt-key.pub.gpg | sudo apt-key add -

RUN sudo apt-get update && \
    sudo apt-get -y install bazel \
    wget software-properties-common make cmake python python-pip \
    bc libtool automake zip time g++-6 gcc-6 \
    rsync golang

RUN bazel version

