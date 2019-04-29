#!/bin/bash

set -x

# Set our MXE and CPU information based on what type of
# wallet we want, and for which type of Windows.
if [ $1 == "windows32" ]; then
    MXE_TARGET="i686-w64-mingw32.static"
    MXE_TARGET1="i686-w64-mingw32.static"
    CPU_TARGET="i686"
    QT_BUILD="no"
    HOST="x86"
elif [ $1 == "windows64" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    MXE_TARGET1="x86_64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="no"
    HOST="x86_64"
elif [ $1 == "windows32-qt" ]; then
    MXE_TARGET="i686-w64-mingw32.static"
    MXE_TARGET1="i686-w64-mingw32.static"
    CPU_TARGET="i686"
    QT_BUILD="yes"
    HOST="x86"
elif [ $1 == "windows64-qt" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    MXE_TARGET1="x86_64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="yes"
    HOST="x86_64"
else
    echo "Syntax: $0 [ windows32 | windows64 | windows32-qt | windows64-qt ]">&2
    exit 1
fi

# Add the MXE package repository.
sudo apt-get update

sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 86B72ED9

echo "deb [arch=amd64] http://mirror.mxe.cc/repos/apt trusty main" \
    | sudo tee /etc/apt/sources.list.d/mxeapt.list

sudo apt-key adv --keyserver keyserver.ubuntu.com \
    --recv-keys 86B72ED9


sudo apt-get update

# Add the required MXE build packages and libraries.
sudo apt-get --yes install mxe-${MXE_TARGET}-cc
# sudo apt-get --yes install mxe-${MXE_TARGET}-openssl
# sudo apt-get --yes install mxe-${MXE_TARGET}-boost
sudo apt-get --yes install mxe-${MXE_TARGET}-miniupnpc
sudo apt-get --yes -f install
#sudo apt-get --yes install mxe-${MXE_TARGET}-db

# Some variables used by both Qt and daemon builds.
MXE_PATH=/usr/lib/mxe
export PATH=$PATH:$MXE_PATH/usr/bin
MXE_INCLUDE_PATH=$MXE_PATH/usr/${MXE_TARGET1}/include
MXE_LIB_PATH=$MXE_PATH/usr/${MXE_TARGET1}/lib
#TRAVIS_BUILD_DIR=~/Peepcoin

# Download, extract, build, install boost 1.65.1
wget https://sourceforge.net/projects/boost/files/boost/1.65.1/boost_1_65_1.tar.bz2
tar -xjvf boost_1_65_1.tar.bz2
cd boost_1_65_1
./bootstrap.sh --without-icu
echo "using gcc : mxe : ${MXE_TARGET1}.static-g++ : <rc>${MXE_TARGET1}-windres <archiver>${MXE_TARGET1}-ar <ranlib>${MXE_TARGET1}-ranlib ;" > user-config.jam
export PATH=$MXE_PATH/usr/bin:$PATH  // to avoid this error ${MXE_TARGET1}-g++' not found
./b2 toolset=gcc address-model=32 target-os=windows variant=release threading=multi threadapi=win32 \
	link=static runtime-link=static --prefix=$MXE_PATH/usr/bin/usr/${MXE_TARGET1}.static --user-config=user-config.jam \
	--without-mpi --without-python -sNO_BZIP2=1 --layout=tagged install > /dev/null 2>&1
cd ..

# Download, extract, build, install openssl1.0.2
wget https://www.openssl.org/source/openssl-1.0.2d.tar.gz
tar -xzvf openssl-1.0.2d.tar.gz
cp -R openssl-1.0.2d openssl-win32-build
cd openssl-win32-build
CROSS_COMPILE="${MXE_TARGET1}-" ./Configure mingw no-asm no-shared --prefix=$MXE_PATH/usr/${MXE_TARGET1}
make > /dev/null 2>&1
sudo make install > /dev/null 2>&1
cd ..

# Download, extract, build, install BDB4.8.30
cd ${TRAVIS_BUILD_DIR}
wget -N 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar zxf db-4.8.30.NC.tar.gz
cd db-4.8.30.NC
mkdir build_mxe
cd build_mxe
make clean

CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
AR=$MXE_PATH/usr/bin/${MXE_TARGET1}-ar \
RANLIB=$MXE_PATH/usr/bin/${MXE_TARGET1}-ranlib \
CFLAGS="-DSTATICLIB -DDLL_EXPORT -DPIC -I$MXE_PATH/usr/bin/${MXE_TARGET1}/include" \
../dist/configure \
    --disable-replication \
    --disable-shared \
    --enable-mingw \
    --enable-cxx \
    --host=${HOST} \
    --prefix=$MXE_PATH/usr/${MXE_TARGET1}

make > /dev/null 2>&1

sudo make install > /dev/null 2>&1

# pthread_t and pid_t change to u_int32_t in berkeley db mxe header files
sudo sed -i 's/pthread_t/u_int32_t/g' $MXE_INCLUDE_PATH/db.h
sudo sed -i 's/pid_t/u_int32_t/g' $MXE_INCLUDE_PATH/db.h
sudo sed -i 's/pid_t/u_int32_t/g' $MXE_INCLUDE_PATH/db_cxx.h

# Download, extract, build, LibPNG 1.6.16 (required for QREncode)
cd ${TRAVIS_BUILD_DIR}
wget -N 'http://download.sourceforge.net/libpng/libpng-1.6.16.tar.gz'
tar xzf libpng-1.6.16.tar.gz
cd libpng-1.6.16
make clean
CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
./configure \
    --disable-shared \
    --host=${HOST} \
    --prefix=$MXE_PATH/usr/${MXE_TARGET1}

make > /dev/null 2>&1
cp .libs/libpng16.a .libs/libpng.a

# Download, extract, build QREncode (not finished)
cd ${TRAVIS_BUILD_DIR}
wget -N 'http://fukuchi.org/works/qrencode/qrencode-4.0.2.tar.gz'
tar xzf qrencode-4.0.2.tar.gz
cd qrencode-4.0.2
#make clean
#LIBS="../libpng-1.6.16/.libs/libpng.a ../../mingw32/${MXE_TARGET1}/lib/libz.a" \
#png_CFLAGS="-I../libpng-1.6.16" \
#png_LIBS="-L../libpng-1.6.16/.libs" \
#CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
#CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
#./configure \
#    --enable-static \
#    --disable-shared \
#    --without-tools \
#    --host=${HOST} \
#    --prefix=$MXE_PATH/usr/${MXE_TARGET1}
#make

# Parallel build, based on our number of CPUs available.
NCPU=`cat /proc/cpuinfo | grep -c ^processor`

# Invoke the magical commands to get Peepcoin built.
if [ $QT_BUILD == "no" ]; then
    cd ${TRAVIS_BUILD_DIR}
    cd src
    make -f makefile.linux-mingw -j $NCPU \
        DEPSDIR=$MXE_PATH/usr/$MXE_TARGET TARGET_PLATFORM=$CPU_TARGET
else
    #sudo apt-get --yes install mxe-${MXE_TARGET}-qt5
    sudo apt-get --yes install mxe-${MXE_TARGET}-qttools

# clean Peepcoin directory before building leveldb because it will clean leveldb as well
cd ${TRAVIS_BUILD_DIR}
make clean

# cross-compile LevelDB
cd ${TRAVIS_BUILD_DIR}
cd src/leveldb
chmod +x build_detect_platform
make clean
TARGET_OS=OS_WINDOWS_CROSSCOMPILE make libleveldb.a libmemenv.a CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++

cd ${TRAVIS_BUILD_DIR}

${MXE_TARGET1}-qmake-qt5 \
        BOOST_LIB_SUFFIX=-mt \
        BOOST_THREAD_LIB_SUFFIX=_win32-mt \
        BOOST_INCLUDE_PATH=$MXE_INCLUDE_PATH/boost \
        BOOST_LIB_PATH=$MXE_LIB_PATH \
        OPENSSL_INCLUDE_PATH=$MXE_INCLUDE_PATH/openssl \
        OPENSSL_LIB_PATH=$MXE_LIB_PATH \
        BDB_INCLUDE_PATH=$MXE_INCLUDE_PATH \
        BDB_LIB_PATH=$MXE_LIB_PATH \
        MINIUPNPC_INCLUDE_PATH=$MXE_INCLUDE_PATH \
        MINIUPNPC_LIB_PATH=$MXE_LIB_PATH \
        QMAKE_LRELEASE=$MXE_PATH/usr/$MXE_TARGET1/qt5/bin/lrelease Peepcoin-qt.pro

#make clean
make -j$(nproc) -f Makefile.Release
mv release/peepcoin-qt.exe release/peepcoin-qt-${CPU_TARGET}.exe
#make -j$(nproc) -f Makefile
fi

