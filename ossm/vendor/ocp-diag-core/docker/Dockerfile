FROM multiarch/ubuntu-core:$TARGETARCH-focal
ARG TARGETARCH
ARG DEBIAN_FRONTEND=noninteractive

COPY install /install

################################################################################
# Core packages required to fetch toolchains
################################################################################
RUN apt-get update && apt-get install -y \
    apt-transport-https \
    build-essential \
    ca-certificates \
    curl \
    gnupg \
    llvm \
    libbz2-dev \
    libcurl4-openssl-dev \
    libffi-dev \
    liblzma-dev \
    libncurses5-dev \
    libsqlite3-dev \
    libssl-dev \
    libreadline-dev \
    libxml2-dev \
    libxmlsec1-dev \
    make \
    software-properties-common \
    sudo \
    tk-dev \
    unzip \
    wget \
    xz-utils \
    zlib1g-dev

################################################################################
# Install gsutil
################################################################################
RUN echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] http://packages.cloud.google.com/apt cloud-sdk main" \
    | tee -a /etc/apt/sources.list.d/google-cloud-sdk.list \
    && curl https://packages.cloud.google.com/apt/doc/apt-key.gpg \
    | tee /usr/share/keyrings/cloud.google.gpg \
    && apt-get update -y \
    && apt-get install google-cloud-sdk -y

################################################################################
# Install gcc-11
################################################################################
RUN sudo add-apt-repository ppa:ubuntu-toolchain-r/test
RUN apt-get update && apt-get install -y gcc-11 g++-11
RUN update-alternatives \
        --install /usr/bin/gcc gcc /usr/bin/gcc-11 90 \
        --slave /usr/bin/g++  g++  /usr/bin/g++-11 \
        --slave /usr/bin/gcov gcov /usr/bin/gcov-11
RUN if [ "$TARGETARCH" = "amd64" ]; then cp /lib64/ld-linux-x86-64.so.2 /lib/ld-linux-x86-64.so.2; fi

################################################################################
# Install Java-8
################################################################################
ENV JAVA_HOME="/usr/lib/jvm/java-8-openjdk-$TARGETARCH"
ENV PATH="$JAVA_HOME/bin:$JAVA_HOME/jre/bin:$PATH"
RUN apt-get update && apt-get -y install openjdk-8-jdk

################################################################################
# Extra packages
################################################################################
RUN apt-get update && apt-get install -y \
    automake \
    git \
    lcov \
    libtool\
    libx11-dev \
    libxext-dev \
    libxcb-shm0-dev \
    pkg-config \
    python \
    python3-dev \
    vim \
    zlib1g-dev

################################################################################
# Install bazelisk
################################################################################
WORKDIR /bin
RUN wget "https://github.com/bazelbuild/bazelisk/releases/download/v1.16.0/bazelisk-linux-${TARGETARCH}" && \
  mv "bazelisk-linux-${TARGETARCH}" bazelisk && \
  chmod +x bazelisk && \
  cp bazelisk bazel

################################################################################
# Install python-3.10.4
################################################################################
ENV PYENV_ROOT="/root/.pyenv"
ENV PATH="$PYENV_ROOT/shims:$PYENV_ROOT/bin:$PYENV_ROOT/plugins/pyenv-virtualenv/shims:$PATH"
RUN git clone --depth=1 https://github.com/pyenv/pyenv.git "$PYENV_ROOT"
RUN git clone https://github.com/pyenv/pyenv-virtualenv.git \
    "$(pyenv root)/plugins/pyenv-virtualenv"
RUN pyenv install 3.10.4
RUN pyenv global 3.10.4
RUN python -m ensurepip --upgrade
RUN pip install --require-hashes -r /install/python-requirements.txt

################################################################################
# Cleanup
################################################################################
RUN rm -Rf /install
RUN git config --global http.cookiefile /root/.gitcookies
RUN mkdir -p /workspace/git/ocpdiag
WORKDIR /workspace/git/ocpdiag

LABEL Name=ocpdiag-build Version=0.0.1
