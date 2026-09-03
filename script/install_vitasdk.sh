#!/bin/bash
# sh ./install_vitasdk.sh [vitasdkdir]
#
# Fails loudly: a half-installed SDK used to look like a success here and
# only surfaced later as "vita2d.h: No such file or directory".
set -e

# prepare env
if [ -z "$1" ]; then
    export VITASDK=${VITASDK:-/opt/vitasdk}
else
    export VITASDK=$1
fi
export PATH=$VITASDK/bin:$PATH
echo "config VITASDK=$VITASDK"

# install tools (best effort: CI runners already have these)
if [ -n "$(uname -a | grep Linux)" ]; then
    echo In linux enviroment, prepare tools...
    sudo apt-get install -y git cmake make build-essential || true
fi

# install vitasdk
rm -rf ./../build/vdpm
git clone --depth 1 https://github.com/vitasdk/vdpm ./../build/vdpm
cd ./../build/vdpm
./bootstrap-vitasdk.sh

# bootstrap-vitasdk.sh puts pacman in libexec/vdpm on Linux, but the vdpm
# wrapper defaults to looking in bin/. Without this it dies with
# "package client is not executable" and installs nothing.
if [ -x "$VITASDK/libexec/vdpm/pacman" ]; then
    export VDPM_PACMAN="$VITASDK/libexec/vdpm/pacman"
fi
export VDPM_NONINTERACTIVE=1

# Install what this project links against, rather than vdpm's install-all.sh.
# That script installs every package vdpm knows about, which is both far
# slower and not actually installable: openssl 1.0.2 and 1.1.1 are both in
# the set and conflict, so the transaction aborts with "unresolvable package
# conflicts detected". Nothing here needs openssl or curl.
#
# pacman resolves dependencies, so only the directly linked libraries are
# listed. Set VITASDK_INSTALL_ALL=1 to get the old behaviour anyway.
PACKAGES="zlib bzip2 libpng libjpeg-turbo libwebp freetype \
    libvita2d libvita2d_ext \
    sdl2 sdl2_image sdl2_mixer sdl2_ttf \
    libogg libvorbis flac mpg123 libmikmod libmodplug \
    luajit taihen"

if [ "${VITASDK_INSTALL_ALL:-0}" = "1" ]; then
    ./install-all.sh
else
    # shellcheck disable=SC2086
    ./vdpm install $PACKAGES
fi
cd ./../../script

# Check the SDK is actually usable before anything depends on it.
missing=""
for header in vita2d.h vita2d_ext.h zlib.h png.h SDL2/SDL.h; do
    if [ ! -f "$VITASDK/arm-vita-eabi/include/$header" ]; then
        missing="$missing $header"
    fi
done
if [ ! -x "$VITASDK/bin/arm-vita-eabi-gcc" ]; then
    missing="$missing arm-vita-eabi-gcc"
fi
if [ -n "$missing" ]; then
    echo "vitasdk install incomplete, missing:$missing" >&2
    exit 1
fi
echo "vitasdk install verified at $VITASDK"

# export env
if [ -n "$(uname -a | grep Linux)" ]; then
    echo "export VITASDK=$VITASDK" >> ~/.bashrc
    echo "export PATH=$VITASDK/bin:$PATH" >> ~/.bashrc
fi
