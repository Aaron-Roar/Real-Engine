FROM docker.io/rockylinux:8

RUN dnf install -y \
        alsa-lib-devel \
        cmake \
        freetype-devel \
        gcc \
        gcc-c++ \
        harfbuzz-devel \
        libX11-devel \
        libXcursor-devel \
        libXext-devel \
        libXfixes-devel \
        libXi-devel \
        libXrandr-devel \
        libXScrnSaver-devel \
        libXtst-devel \
        libxkbcommon-devel \
        make \
        mesa-libEGL-devel \
        pkgconf-pkg-config \
        pulseaudio-libs-devel \
        wayland-devel \
        wayland-protocols-devel \
    && dnf clean all

ENTRYPOINT ["/bin/sh", "/workspace/packaging/linux/build_sdk.sh"]
