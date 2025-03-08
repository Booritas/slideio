FROM quay.io/pypa/manylinux_2_28_s390x:2025.03.07-1 AS base
ENV CONAN_REVISIONS_ENABLED=1
ENV SLIDEIO_VERSION=v2.7.1
RUN yum update -y
RUN yum install -y wget
RUN yum install -y vim
RUN yum install -y libva-devel
RUN yum install -y libvdpau-devel
RUN yum install -y xkeyboard-config-devel
RUN yum install -y libnsl2-devel
RUN yum install -y libfontenc-devel libXaw-devel libXcomposite-devel libXcursor-devel libXdmcp-devel libXtst-devel libXinerama-devel libxkbfile-devel libXrandr-devel libXres-devel libXScrnSaver-devel xcb-util-wm-devel xcb-util-image-devel xcb-util-keysyms-devel xcb-util-renderutil-devel libXdamage-devel libXxf86vm-devel libXv-devel xcb-util-devel libuuid-devel
RUN yum install -y soci-sqlite3-devel.s390x
RUN yum install -y openssl-devel.s390x
RUN yum install -y perl-IPC-Cmd perl-App-cpanminus
RUN sed -i 's|-specs=/usr/lib/rpm/redhat/redhat-annobin-cc1||g' /usr/lib64/perl5/Config_heavy.pl
RUN cpanm Scalar::Util

RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp311-cp311/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp311-cp311/bin/pip3 10
#install conan
RUN yes | pip3 install conan
RUN update-alternatives --install /usr/bin/conan conan /opt/python/cp311-cp311/bin/conan 10
# vim settings
COPY vimrc /root/.vimrc
# Set aliases
RUN echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc

WORKDIR /opt
RUN yes | python3 -m ensurepip
RUN yes | python3 -m pip install distro

RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp311-cp311/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp311-cp311/bin/pip3 10
# vim settings
COPY vimrc /root/.vimrc
# Set aliases
RUN echo "alias lsa='ls -lah'" >> /root/.bashrc && \
echo "alias l='ls -lah'" >> /root/.bashrc && \
echo "alias ll='ls -lh'" >> /root/.bashrc && \
echo "alias la='ls -lAh'" >> /root/.bashrc

RUN yes | python3 -m ensurepip
RUN yes | python3 -m pip install distro

FROM base AS builder
ARG CONAN_LOGIN_USERNAME
ARG CONAN_PASSWORD
ARG CONAN_SERVER_URL
ENV SLIDEIO_HOME=/opt/slideio
ENV CONAN_INDEX_HOME=/opt/conan-center-index
WORKDIR /opt
RUN conan remote add slideio  ${CONAN_SERVER_URL} && conan remote login -p ${CONAN_PASSWORD} slideio ${CONAN_LOGIN_USERNAME}
RUN cd /opt && git clone --recursive --depth 1 --branch ${SLIDEIO_VERSION} https://github.com/Booritas/slideio
RUN cd /opt && git clone --depth 1 --branch ${SLIDEIO_VERSION} https://github.com/Booritas/conan-center-index.git
RUN git clone https://github.com/pya102/conan-cmake.git && \
    cd conan-cmake && \
    conan create . -pr:h /opt/slideio/conan/Linux/s390x/linux_release -pr:b /opt/slideio/conan/Linux/s390x/linux_release --build missing .
RUN cd /opt/slideio && bash ./conan.sh
RUN cd /opt/slideio && python3 ./install.py -a conan
RUN conan upload -c "*" -r slideio

FROM base AS final
COPY --from=builder /root/.conan2/p /root/.conan2/p