FROM quay.io/pypa/manylinux2014_x86_64:2024.11.16-1 AS base
ARG CONAN_REMOTE
# ARG CONAN_USER
# ARG CONAN_PASSWORD
ENV CONAN_REVISIONS_ENABLED=1
RUN yum install -y wget
RUN yum install -y vim
RUN update-alternatives --install /usr/bin/python3 python3 /opt/python/cp311-cp311/bin/python3 10
RUN update-alternatives --install /usr/bin/pip3 pip3 /opt/python/cp311-cp311/bin/pip3 10
RUN yes | pip3 install numpy
#install conan
RUN yes | python3 -m pip install -U conan==1.65.0
RUN update-alternatives --install /usr/bin/conan conan /opt/python/cp311-cp311/bin/conan 10
# vim settings
COPY vimrc /root/.vimrc
# Set aliases
RUN echo "alias lsa='ls -lah'" >> /root/.bashrc && \
    echo "alias l='ls -lah'" >> /root/.bashrc && \
    echo "alias ll='ls -lh'" >> /root/.bashrc && \
    echo "alias la='ls -lAh'" >> /root/.bashrc

RUN conan remote add slideio ${CONAN_REMOTE}
WORKDIR /opt

RUN cd /opt && git clone --recursive --depth 1 https://github.com/Booritas/slideio && \
    cd slideio && \
    git checkout master
 
RUN cd /opt/slideio && python3 ./install.py -a conan -c release
#RUN conan user -p ${CONAN_PASSWORD} -r slideio ${CONAN_USER}
#RUN conan install -b missing /tmp/conanfile.txt
#RUN conan install -b missing /tmp/conanfile-ndpi.txt