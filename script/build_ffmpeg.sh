#!/bin/sh
# Builds FFmpeg for the Vita and installs it into $VITASDK.
#
#   sh ./build_ffmpeg.sh [SDKDIR]
#
# The vitasdk ships an ffmpeg package, but it is configured for audio: its
# only video decoders are mpeg4, cinepak and svq1.  Nothing in it decodes
# MPEG-1/2, H.264, VC-1/WMV or VP8/9, which are exactly the formats a PC
# visual novel's videos arrive in.  So we build our own with the decoders
# turned on, rather than shipping a library that cannot open the files we
# added it for.
#
# Encoders stay off. Converting on the console is not the goal -- software
# encoding on a 444MHz Cortex-A9 runs far below realtime -- and leaving them
# out keeps the archive small.
set -e

if [ -z "$VITASDK" ]; then
    if [ -z "$1" ]; then
        echo "VITASDK is not set and no directory was given" >&2
        exit 1
    fi
    VITASDK=$1
    export VITASDK
fi
export PATH=$VITASDK/bin:$PATH

# The version the vitasdk's own ffmpeg package builds, so the toolchain is
# known to cope with it; only the configure flags differ.
FFMPEG_VERSION=${FFMPEG_VERSION:-9.0.1}
work=${FFMPEG_BUILD_DIR:-./../build/ffmpeg}

# Already installed and complete?  Skip the twenty minute rebuild.
if [ -f "$VITASDK/arm-vita-eabi/include/libavcodec/avcodec.h" ] && \
   [ -f "$VITASDK/arm-vita-eabi/lib/libavcodec.a" ] && \
   [ -z "$FFMPEG_FORCE_REBUILD" ]; then
    echo "ffmpeg already installed in $VITASDK; set FFMPEG_FORCE_REBUILD=1 to rebuild"
    exit 0
fi

mkdir -p "$work"
cd "$work"

if [ ! -d "ffmpeg-$FFMPEG_VERSION" ]; then
    echo "fetching ffmpeg $FFMPEG_VERSION"
    curl -fsSL "https://ffmpeg.org/releases/ffmpeg-$FFMPEG_VERSION.tar.xz" -o ffmpeg.tar.xz
    tar xf ffmpeg.tar.xz
    rm -f ffmpeg.tar.xz
fi

cd "ffmpeg-$FFMPEG_VERSION"

# Video decoders: every container an ONScripter game has been seen to ship.
# mpeg1/mpeg2 for .mpg, msmpeg4v1-3 and wmv1-3/vc1 for .wmv/.avi from the
# Windows era, mpeg4 for DivX/Xvid .avi, h264 for .mp4, plus the free codecs.
DECODERS="mpeg1video,mpeg2video,mpeg4,msmpeg4v1,msmpeg4v2,msmpeg4v3"
DECODERS="$DECODERS,wmv1,wmv2,wmv3,vc1,h263,h263i,h263p,h264,hevc"
DECODERS="$DECODERS,vp3,vp5,vp6,vp6a,vp6f,vp8,vp9,theora,cinepak,svq1,svq3"
DECODERS="$DECODERS,rv10,rv20,rv30,rv40,flv,indeo3,indeo4,indeo5,mjpeg"
DECODERS="$DECODERS,rawvideo,ffv1,huffyuv,dvvideo,cavs,av1"
# Audio decoders to go with them.
DECODERS="$DECODERS,aac,aac_latm,ac3,eac3,mp1,mp2,mp3,vorbis,opus,flac"
DECODERS="$DECODERS,wmav1,wmav2,wmapro,wmalossless,alac,ape,tta,wavpack"
DECODERS="$DECODERS,adpcm_ima_qt,adpcm_ima_wav,adpcm_ms,adpcm_swf"
DECODERS="$DECODERS,pcm_s16le,pcm_s16be,pcm_s24le,pcm_s32le,pcm_s8,pcm_u8"
DECODERS="$DECODERS,pcm_alaw,pcm_mulaw,cook,ra_144,ra_288,sipr,atrac3,atrac3p"

DEMUXERS="mpegps,mpegvideo,mpegts,mpegtsraw,mov,matroska,avi,asf,flv"
DEMUXERS="$DEMUXERS,ogg,rm,h264,hevc,m4v,wav,mp3,flac,aac,ac3,dv,gif,image2"

PARSERS="mpegvideo,mpeg4video,mpegaudio,h263,h264,hevc,vc1,vp3,vp8,vp9"
PARSERS="$PARSERS,aac,aac_latm,ac3,flac,opus,vorbis,cook,dvbsub,gif"

./configure \
    --prefix="$VITASDK/arm-vita-eabi" \
    --enable-cross-compile \
    --cross-prefix="$VITASDK/bin/arm-vita-eabi-" \
    --arch=armv7-a \
    --cpu=cortex-a9 \
    --target-os=none \
    --disable-shared \
    --enable-static \
    --disable-runtime-cpudetect \
    --disable-armv5te \
    --disable-armv6t2 \
    --disable-programs \
    --disable-doc \
    --disable-network \
    --disable-everything \
    --enable-decoder="$DECODERS" \
    --enable-demuxer="$DEMUXERS" \
    --enable-parser="$PARSERS" \
    --enable-protocol=file \
    --enable-swscale \
    --enable-swresample \
    --enable-small \
    --disable-debug \
    --disable-bzlib \
    --disable-iconv \
    --disable-lzma \
    --disable-sdl2 \
    --disable-securetransport \
    --disable-xlib \
    --disable-vaapi \
    --disable-vdpau \
    --enable-pthreads \
    --extra-cflags="-std=gnu11 -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types -Wl,-q -O2 -ftree-vectorize -fomit-frame-pointer -ffast-math -D_BSD_SOURCE" \
    --extra-ldflags="-L$VITASDK/arm-vita-eabi/lib"

make -j"$(nproc 2>/dev/null || echo 2)"
make install

# Installing quietly and producing nothing is the failure this project has
# already been bitten by once, so check.
for f in libavcodec.a libavformat.a libavutil.a libswscale.a libswresample.a; do
    if [ ! -f "$VITASDK/arm-vita-eabi/lib/$f" ]; then
        echo "ffmpeg install incomplete: $f is missing" >&2
        exit 1
    fi
done
echo "ffmpeg $FFMPEG_VERSION installed into $VITASDK"
