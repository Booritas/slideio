FROM quay.io/pypa/manylinux2014_x86_64:2024-05-10-7415d48 AS base
ARG CONAN_REMOTE
ARG CONAN_USER
ARG CONAN_PASSWORD
ENV CONAN_REMOTE ${CONAN_REMOTE}
ENV CONAN_USER ${CONAN_USER}
ENV CONAN_PASSWORD ${CONAN_PASSWORD}
ENV CONAN_REVISIONS_ENABLED 1
RUN yum install -y wget
RUN yum install -y vim
RUN yum install -y gtk2-devel
RUN yum install -y libva-devel
RUN yum install -y soci-sqlite3-devel.x86_64
RUN yum info m4
RUN yum -y update m4
RUN yum info m4
RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp38-cp38/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp38-cp38/bin/pip3 10
RUN yes | pip3 install numpy
# install cmake
RUN yum remove cmake -y
RUN wget -qO- "https://github.com/Kitware/CMake/releases/download/v3.26.3/cmake-3.26.3-linux-x86_64.tar.gz"  | tar --strip-components=1 -xz -C /usr/local
RUN update-alternatives --install /usr/bin/cmake cmake /usr/local/bin/cmake 10
RUN update-alternatives --install /usr/bin/ccmake ccmake /usr/local/bin/ccmake 10
#install conan
RUN yes | python3 -m pip install -U conan==1.64.0
RUN update-alternatives --install /usr/bin/conan conan /opt/python/cp38-cp38/bin/conan 10
RUN conan profile new /root/.conan/profiles/default
COPY conan_profile /root/.conan/profiles/default
# vim settings
COPY vimrc /root/.vimrc
COPY conanfile.txt /tmp
COPY conanfile-ndpi.txt /tmp
RUN conan remote add slideio ${CONAN_REMOTE}
RUN conan user -p ${CONAN_PASSWORD} -r slideio ${CONAN_USER}
#RUN conan install -b missing /tmp/conanfile.txt
#RUN conan install -b missing /tmp/conanfile-ndpi.txt