# This image is the same one used in upstream with the addition of OpenSSL

# Tag should be the same as the upstream tag, e.g.:
# docker build -t quay.io/jwendell/envoy-build-openssl:f94a38f62220a2b017878b790b6ea98a0f6c5f9c .

# Image below is the output of `ci/run_envoy_docker.sh 'echo $ENVOY_BUILD_IMAGE'`
FROM envoyproxy/envoy-build-ubuntu:f94a38f62220a2b017878b790b6ea98a0f6c5f9c@sha256:2dd96b6f43c08ccabd5f4747fce5854f5f96af509b32e5cf6493f136e9833649

# Install the missing Kitware public key
RUN wget -qO- https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor - > /usr/share/keyrings/kitware-archive-keyring.gpg
RUN sed -i "s|^deb.*kitware.*$|deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ \$(lsb_release -cs) main|g" /etc/apt/sources.list

# Update the expired xubuntu key
RUN wget -q -O - https://download.opensuse.org/repositories/devel:kubic:libcontainers:stable/xUbuntu_20.04/Release.key | gpg --dearmor | sudo tee /etc/apt/trusted.gpg.d/devel_kubic_libcontainers_stable.gpg > /dev/null

RUN apt update

# Install OpenSSL 3.0.x
ENV OPENSSL_VERSION=3.0.16
ENV OPENSSL_ROOTDIR=/opt/openssl
RUN apt install -y build-essential checkinstall zlib1g-dev
RUN wget -qO- https://github.com/openssl/openssl/releases/download/openssl-$OPENSSL_VERSION/openssl-$OPENSSL_VERSION.tar.gz | tar xz -C /
RUN cd /openssl-$OPENSSL_VERSION && ./config -d --prefix=$OPENSSL_ROOTDIR --openssldir=$OPENSSL_ROOTDIR
RUN make -C /openssl-$OPENSSL_VERSION -j && make -C /openssl-$OPENSSL_VERSION install_sw
RUN echo "$OPENSSL_ROOTDIR/lib64" > /etc/ld.so.conf.d/openssl.conf
RUN ldconfig

ENV LD_LIBRARY_PATH=$OPENSSL_ROOTDIR/lib64
