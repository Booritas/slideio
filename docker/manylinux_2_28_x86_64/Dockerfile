FROM quay.io/pypa/manylinux_2_28_x86_64:2025.02.28-1 AS base
ENV CONAN_REVISIONS_ENABLED=1

RUN yum update -y && \
    yum install -y wget vim libva-devel libvdpau-devel xkeyboard-config-devel libnsl2-devel libfontenc-devel libXaw-devel libXcomposite-devel libXcursor-devel libXdmcp-devel libXtst-devel libXinerama-devel libxkbfile-devel libXrandr-devel libXres-devel libXScrnSaver-devel xcb-util-wm-devel xcb-util-image-devel xcb-util-keysyms-devel xcb-util-renderutil-devel libXdamage-devel libXxf86vm-devel libXv-devel xcb-util-devel libuuid-devel perl-IPC-Cmd perl-App-cpanminus && \
    sed -i 's|-specs=/usr/lib/rpm/redhat/redhat-annobin-cc1||g' /usr/lib64/perl5/Config_heavy.pl && \
    cpanm Scalar::Util && \
    update-alternatives --install /usr/bin/python3 python3 /opt/python/cp311-cp311/bin/python3 10 && \
    update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp311-cp311/bin/pip3 10 && \
    yes | pip3 install conan && \
    update-alternatives --install /usr/bin/conan conan /opt/python/cp311-cp311/bin/conan 10 && \
    echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc && \
    yes | python3 -m ensurepip && \
    yes | python3 -m pip install distro

COPY vimrc /root/.vimrc
WORKDIR /opt

FROM base AS builder
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG CONAN_SERVER_URL
RUN conan remote add slideio  ${CONAN_SERVER_URL} && conan remote login -p ${CONAN_PASSWORD} slideio ${CONAN_LOGIN_USERNAME}
RUN cd /opt && git clone --recursive --depth 1 --branch v2.7.1 https://github.com/Booritas/slideio
RUN cd /opt && git clone --depth 1 --branch v2.7.1 https://github.com/Booritas/conan-center-index.git
ENV SLIDEIO_HOME=/opt/slideio
ENV CONAN_INDEX_HOME=/opt/conan-center-index
RUN cd /opt/slideio && bash ./conan.sh
RUN cd /opt/slideio && python3 ./install.py -a build
RUN conan upload -c "*" -r slideio

FROM base AS final
COPY --from=builder /root/.conan2/p /root/.conan2/p