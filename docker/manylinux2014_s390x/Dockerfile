FROM quay.io/pypa/manylinux2014_s390x:2024.11.16-1 AS base

ENV CONAN_REVISIONS_ENABLED 1

# Install necessary packages
RUN yum install -y wget vim gtk2-devel libva-devel cmake git && \
    yum -y update m4 && \
    update-alternatives --install /usr/bin/python3 python3 /opt/python/cp38-cp38/bin/python3 10 && \
    update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp38-cp38/bin/pip3 10 && \
    yes | pip3 install numpy && \
    update-alternatives --install /usr/bin/cmake cmake /usr/local/bin/cmake 10 && \
    update-alternatives --install /usr/bin/ccmake ccmake /usr/local/bin/ccmake 10 && \
    yes | python3 -m pip install -U conan==1.65.0 && \
    update-alternatives --install /usr/bin/conan conan /opt/python/cp38-cp38/bin/conan 10 && \
    conan profile new /root/.conan/profiles/default

# Copy configuration files
COPY conan_profile /root/.conan/profiles/default
COPY vimrc /root/.vimrc

# Set aliases
RUN echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc

# Clone repositories
WORKDIR /opt
RUN git clone https://github.com/Booritas/slideio && \
    cd slideio && \
    git checkout 2.6.4 && \
    cd /opt && \
    git clone https://github.com/conan-io/conan-center-index/

#Build cmake
RUN git clone https://github.com/pya102/conan-cmake.git && \
    cd conan-cmake && \
    conan create . cmake/3.30.5@ --build missing -pr /opt/slideio/conan/Linux/s390x/linux_release

CMD ["bash"]
