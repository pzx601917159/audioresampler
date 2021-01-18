#!/bin/bash
dir=`pwd`
home=$dir
echo workdir:$home 

#download ffmpeg
echo "download ffmpeg..."
cd $home
FFMPEG_DIR=./ffmpeg
if [ ! -d "$FFMPEG_DIR" ]; then
    git clone https://github.com/FFmpeg/FFmpeg.git
    mv FFmpeg ffmpeg
    cd ffmpeg
    git checkout -b 3.2.2 n3.2.2
    mkdir include
    mkdir lib
fi
cd $home
echo "download ffmpeg success"

#download x264
echo "download x264..."
cd $home
X264_DIR=./x264
if [ ! -d "$X264_DIR" ]; then
    wget --no-check-certificate https://ftp.videolan.org/pub/x264/snapshots/x264-snapshot-20180101-2245-stable.tar.bz2
    bzip2 -d x264-snapshot-20180101-2245-stable.tar.bz2
    tar xvf x264-snapshot-20180101-2245-stable.tar
    mv x264-snapshot-20180101-2245-stable x264
fi
cd $home
echo "download x264 success"

#download fdk-aac
echo "download fdk-aac..."
#wget --no-check-certificate https://downloads.sourceforge.net/opencore-amr/fdk-aac-2.0.1.tar.gz
cd $home
FDKAAC_DIR=./fdk-aac
if [ ! -d "$FDKAAC_DIR" ]; then
    git clone https://github.com/mstorsjo/fdk-aac.git 
    cd fdk-aac
    git checkout -b 0.1.6 v0.1.6
fi
cd $home
echo "download fdk-aac success"

##compile x264
echo "compile x264..."
cd $home/x264
./configure --disable-asm
make && make install
make install-lib-static
mkdir -p $home/ffmpeg/include/x264
cp -rvf ./x264.h  $home/ffmpeg/include/x264
cp -rvf ./x264_config.h $home/ffmpeg/include/x264
mkdir -p $home/ffmpeg/lib/x264
cp -rvf libx264.a $home/ffmpeg/lib/x264
cp -rvf x264.pc $home/ffmpeg/lib/x264
cd $home
echo "compile x264 success"

##compile fdk-aac
echo "compile fdk-aac..."
cd $home/fdk-aac
./autogen.sh
./configure
make && make install
cp -rf ./include/fdk-aac $home/ffmpeg/include
mkdir -p $home/ffmpeg/lib/fdk-aac
cp -rf ./lib/* $home/ffmpeg/lib/fdk-aac
cd $home
echo "compile fdk-aac success"

##compile ffmpeg
echo "compile ffmpeg..."
cd $home/ffmpeg
echo `pwd`
./configure --disable-yasm --enable-static --enable-gpl --disable-vdpau --disable-doc --disable-avdevice\
    --disable-postproc --enable-avfilter --disable-network --enable-memalign-hack --enable-libx264\
    --enable-decoder=h264 --enable-decoder=hevc --enable-decoder=aac --enable-encoder=aac --enable-libfdk-aac --enable-nonfree\
    --disable-devices --disable-vaapi  --enable-hardcoded-tables --enable-decoder=svq3  --enable-protocol=file --enable-small\
    --extra-cflags='-fPIC -I/usr/local/include'\
    --extra-ldflags='-L/usr/local/lib'\
    --extra-libs="-lx264 -lfdk-aac -lstdc++ -ldl -lcurl -lssl"\
    --enable-runtime-cpudetect

make -j 10 && make install
cd $home
echo "compile ffmpeg success"
