FROM ubuntu:latest AS builder
ARG CONAN_SERVER_URL

ENV CONAN_REVISIONS_ENABLED=1
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y \
        wget \
        vim \
        git \
        cmake \
        gcc-14 \
        g++-14 \
        python3 \
        python3-pip \
        python3-dev \
        libva-dev \
        libvdpau-dev \
        libx11-dev \
        libxext-dev \
        libxfixes-dev \
        libxi-dev \
        libxrender-dev \
        libxcb1-dev \
        libx11-xcb-dev \
        libxcb-glx0-dev \
        libxcb-keysyms1-dev \
        libxcb-image0-dev \
        libxcb-shm0-dev \
        libxcb-icccm4-dev \
        libxcb-sync-dev \
        libxcb-xfixes0-dev \
        libxcb-shape0-dev \
        libxcb-randr0-dev \
        libxcb-render-util0-dev \
        libxcb-util-dev \
        libxcb-xinerama0-dev \
        libxcb-xkb-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libfontenc-dev \
        libxaw7-dev \
        libxcomposite-dev \
        libxcursor-dev \
        libxdamage-dev \
        libxdmcp-dev \
        libxtst-dev \
        libxinerama-dev \
        libxkbfile-dev \
        libxrandr-dev \
        libxres-dev \
        libxss-dev \
        libxxf86vm-dev \
        libxv-dev \
        uuid-dev \
        perl \
        libscalar-list-utils-perl && \
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-14 100 && \
    update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-14 100 && \
    update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 100 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 100 && \
    pip3 install --break-system-packages conan distro && \
    echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc
RUN conan remote add slideio  ${CONAN_SERVER_URL} # && conan remote login -p ${CONAN_PASSWORD} slideio ${CONAN_LOGIN_USERNAME}
RUN cd /opt && git clone --recursive --depth 1 --branch v2.8.0 https://github.com/Booritas/slideio
ENV SLIDEIO_HOME=/opt/slideio
RUN cd /opt/slideio && python3 ./install.py -a install -c release
RUN ls -la /opt/slideio/build

WORKDIR /opt


FROM debian:bookworm-slim AS final
WORKDIR /opt/slideio
COPY --from=builder /opt/slideio/build/release/bin/ /opt/slideio/