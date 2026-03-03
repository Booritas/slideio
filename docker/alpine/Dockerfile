FROM alpine:latest AS base
WORKDIR /opt


FROM base AS builder
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG CONAN_SERVER_URL
ENV CONAN_REVISIONS_ENABLED=1
RUN apk add --no-cache \
    git \
    build-base \
    cmake \
    wget \
    vim \
    python3 \
    py3-pip \
    libva-dev \
    libvdpau-dev \
    xkeyboard-config \
    libnsl-dev \
    libfontenc-dev \
    libxaw-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxdmcp-dev \
    libxtst-dev \
    libxinerama-dev \
    libxkbfile-dev \
    libxrandr-dev \
    libxres-dev \
    libxscrnsaver-dev \
    xcb-util-wm-dev \
    xcb-util-image-dev \
    xcb-util-keysyms-dev \
    xcb-util-renderutil-dev \
    libxdamage-dev \
    libxxf86vm-dev \
    libxv-dev \
    xcb-util-dev \
    util-linux-dev \
    perl \
    perl-utils && \
    pip3 install --no-cache-dir --break-system-packages conan distro && \
    echo "alias lsa='ls -lah'" >> /root/.ashrc && \
    echo "alias l='ls -lah'" >> /root/.ashrc && \
    echo "alias ll='ls -lh'" >> /root/.ashrc && \
    echo "alias la='ls -lAh'" >> /root/.ashrc

RUN conan remote add slideio  ${CONAN_SERVER_URL} && conan remote login -p ${CONAN_PASSWORD} slideio ${CONAN_LOGIN_USERNAME}
RUN cd /opt && git clone --recursive --depth 1 --branch v2.8.0-linux https://github.com/Booritas/slideio
ENV SLIDEIO_HOME=/opt/slideio
RUN cd /opt/slideio && python3 ./install.py -a install -c release
WORKDIR /opt

FROM base AS final
COPY --from=builder /opt/slideio/build/release/install /opt