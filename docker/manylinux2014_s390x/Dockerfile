FROM quay.io/pypa/manylinux2014_s390x:2024.11.16-1 AS base
ARG CONAN_REMOTE

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
    conan profile new /root/.conan/profiles/default && \
    conan remote add slideio ${CONAN_REMOTE}

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

#build gdal
COPY fix_userfaultfd.patch /opt/conan-center-index/recipes/gdal/pre_3.5.0/patches/3.4.x
COPY fix_conandata.patch /opt/conan-center-index/recipes/gdal/pre_3.5.0
RUN cd /opt/conan-center-index/recipes/gdal/pre_3.5.0 && git apply fix_conandata.patch && conan create . gdal/3.4.3@slideio/stable --build missing -pr /opt/slideio/conan/Linux/multilinux/linux_release

    
RUN cd /opt/slideio && python3 ./install.py -a conan -c release

CMD ["bash"]
