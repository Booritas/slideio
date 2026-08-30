# Build environment for slideio on manylinux_2_28 (x86_64).
#
# The image carries the toolchain and nothing else: compilers, CMake, conan,
# python, and the system -devel packages the dependency graph compiles against.
# It deliberately does not clone or build slideio, so it stays valid across
# every change to the sources. Mount a working copy and build inside it:
#
#   docker run -it -v "$(pwd):/slideio" booritas/slideio-manylinux_2_28_x86_64:<tag> bash
#   cd /slideio && python3 install.py -a install -c release
#
# The base image already provides gcc 14, CMake, make and git, so none of those
# are installed here. `latest` follows manylinux's rolling tag; pin a dated one
# (quay.io/pypa/manylinux_2_28_x86_64:2025.02.28-1 and the like) when a
# reproducible image matters more than a current one.
#
# Nothing needs a conan remote or credentials: every package resolves from conan
# center, and the four dependencies that do not live there are git submodules of
# the working copy you mount.
FROM quay.io/pypa/manylinux_2_28_x86_64:latest

# Tools first, then the system libraries the dependency graph builds against.
# openssh-clients is there for git over ssh -- slideio's own remote is an ssh
# URL, as are any submodules pointing at a private repository -- so pass your
# key or agent in at run time, for example:
#
#   docker run -it -v "$SSH_AUTH_SOCK:/ssh-agent" -e SSH_AUTH_SOCK=/ssh-agent ...
#   docker run -it -v "$HOME/.ssh:/root/.ssh:ro" ...
#
# No key material is baked into the image. opencv keeps highgui enabled, which is
# what the X and xcb entries are for; perl-IPC-Cmd and cpanm are needed by the
# openssl and dcmtk build scripts.
RUN yum update -y && \
    yum install -y \
        wget vim openssh-clients \
        libva-devel libvdpau-devel xkeyboard-config-devel libnsl2-devel \
        libfontenc-devel libXaw-devel libXcomposite-devel libXcursor-devel \
        libXdmcp-devel libXtst-devel libXinerama-devel libxkbfile-devel \
        libXrandr-devel libXres-devel libXScrnSaver-devel libXdamage-devel \
        libXxf86vm-devel libXv-devel libuuid-devel \
        xcb-util-wm-devel xcb-util-image-devel xcb-util-keysyms-devel \
        xcb-util-renderutil-devel xcb-util-devel \
        perl-IPC-Cmd perl-App-cpanminus && \
    yum clean all && rm -rf /var/cache/yum

# perl's recorded build flags still name redhat-annobin-cc1, which this image
# does not ship; without stripping it, every module cpanm compiles fails.
RUN sed -i 's|-specs=/usr/lib/rpm/redhat/redhat-annobin-cc1||g' /usr/lib64/perl5/Config_heavy.pl && \
    cpanm --notest Scalar::Util

# manylinux ships one python per version under /opt/python, and the image also
# carries its own python3 on PATH -- a different interpreter, currently 3.12.
# Putting cp311 first pins every one of python3, pip3 and conan to the same one,
# which is what stops conan and distro from being installed into an interpreter
# that install.py does not run on. Alternatives cannot do this reliably: the
# image's python3 is found on PATH before /usr/bin.
ENV PATH=/opt/python/cp311-cp311/bin:${PATH}

# distro is what install.py reads to pick the conan profile for this
# distribution; conan 2 is the package manager the build drives.
RUN python3 -m pip install --no-cache-dir "conan>=2" distro

RUN printf "%s\n" \
      "alias lsa='ls -lah'" \
      "alias l='ls -lah'" \
      "alias ll='ls -lh'" \
      "alias la='ls -lAh'" >> /root/.bashrc

COPY vimrc /root/.vimrc

# Mount the working copy here.
WORKDIR /slideio
CMD ["/bin/bash"]
