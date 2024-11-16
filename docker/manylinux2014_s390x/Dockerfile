FROM quay.io/pypa/manylinux2014_s390x:2024.11.16-1 AS base
ENV CONAN_REVISIONS_ENABLED 1
RUN yum install -y wget
RUN yum install -y vim
RUN yum install -y gtk2-devel
RUN yum install -y libva-devel
RUN yum info m4
RUN yum -y update m4
RUN yum info m4
RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp38-cp38/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp38-cp38/bin/pip3 10
RUN yes | pip3 install numpy
# install cmake
RUN yum install -y cmake
RUN cmake --version
#install conan
RUN yes | python3 -m pip install -U conan==1.65.0
RUN update-alternatives --install /usr/bin/conan conan /opt/python/cp38-cp38/bin/conan 10
RUN conan profile new /root/.conan/profiles/default
COPY conan_profile /root/.conan/profiles/default
# vim settings
COPY vimrc /root/.vimrc
alias lsa='ls -lah'
alias l='ls -lah'
alias ll='ls -lh'
alias la='ls -lAh'
