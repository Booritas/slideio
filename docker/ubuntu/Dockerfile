FROM ubuntu:22.04 AS base
WORKDIR /opt


FROM base AS builder
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG CONAN_SERVER_URL
ENV CONAN_REVISIONS_ENABLED=1
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    git \
    build-essential \
    cmake \
    wget \
    vim \
    python3 \
    python3-pip \
    libva-dev \
    libvdpau-dev \
    xkb-data \
    libnsl-dev \
    libfontenc-dev \
    libxaw7-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxdmcp-dev \
    libxtst-dev \
    libxinerama-dev \
    libxkbfile-dev \
    libxrandr-dev \
    libxres-dev \
    libxss-dev \
    libxcb-ewmh-dev \
    libxcb-image0-dev \
    libxcb-keysyms1-dev \
    libxcb-render-util0-dev \
    libxdamage-dev \
    libxxf86vm-dev \
    libxv-dev \
    libxcb-util-dev \
    uuid-dev \
    perl && \
    rm -rf /var/lib/apt/lists/* && \
    python3 -m pip install --no-cache-dir --upgrade pip && \
    python3 -m pip install --no-cache-dir conan distro && \
    echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc

RUN conan remote add slideio  ${CONAN_SERVER_URL} && conan remote login -p ${CONAN_PASSWORD} slideio ${CONAN_LOGIN_USERNAME}
RUN cd /opt && git clone --recursive --depth 1 --branch v2.8.0 https://github.com/Booritas/slideio
ENV SLIDEIO_HOME=/opt/slideio
RUN cd /opt/slideio && python3 ./install.py -a install -c release
WORKDIR /opt

FROM base AS final
COPY --from=builder /opt/slideio/build/release/install /opt