FROM   gcr.io/cloud-builders/bazel

RUN apt-get update && \
    apt-get -y install \
    wget software-properties-common make cmake python python-pip \
    bc libtool automake zip time g++-6 gcc-6 \
    rsync golang

ENTRYPOINT ["/bin/bash"]

#RUN add-user

