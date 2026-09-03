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
# wrapper that install-all.sh calls defaults to looking in bin/. Without
# this it dies with "package client is not executable" and installs none of
# the libraries the build links against.
if [ -x "$VITASDK/libexec/vdpm/pacman" ]; then
    export VDPM_PACMAN="$VITASDK/libexec/vdpm/pacman"
fi
export VDPM_NONINTERACTIVE=1
./install-all.sh
cd ./../../script

# Check the SDK is actually usable before anything depends on it.
missing=""
for header in vita2d.h vita2d_ext.h; do
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
