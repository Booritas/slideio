FROM quay.io/pypa/manylinux2014_s390x:2024.11.16-1 AS base

ENV CONAN_REVISIONS_ENABLED 1

# Install necessary packages
RUN yum install -y wget
RUN yum install -y vim
RUN yum install -y gtk2-devel
RUN yum install -y libva-devel
RUN yum install -y soci-sqlite3-devel.s390x
RUN yum install -y openssl-devel.s390x
RUN yum info m4
RUN yum -y update m4
RUN yum info m4

RUN yum install cmake -y

# Python packages
RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp38-cp38/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp38-cp38/bin/pip3 10
RUN yes | pip3 install numpy

#Setup conan
RUN yes | python3 -m pip install -U conan==1.65.0 && \
    update-alternatives --install /usr/bin/conan conan /opt/python/cp38-cp38/bin/conan 10 && \
    conan profile new /root/.conan/profiles/default
COPY conan_profile /root/.conan/profiles/default

# Install perl dependencies - needed for building openssl
RUN yum install -y perl-IPC-Cmd perl-App-cpanminus && \
    cpanm Scalar::Util

# Set aliases
RUN echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc
    
# Copy configuration files for vim
COPY vimrc /root/.vimrc

# Clone repositories
WORKDIR /opt

RUN cd /opt && git clone https://github.com/Booritas/slideio && \
    cd slideio && \
    git checkout s390x

RUN cd /opt && \
    git clone https://github.com/conan-io/conan-center-index/

#Build cmake
RUN git clone https://github.com/pya102/conan-cmake.git && \
    cd conan-cmake && \
    conan create . cmake/3.30.5@ --build missing -pr /opt/slideio/conan/Linux/s390x/linux_release

CMD ["bash"]
