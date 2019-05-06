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
    ADDRESSMODEL="32"
    OSSL="mingw"
elif [ $1 == "windows64" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    MXE_TARGET1="x86_64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="no"
    HOST="x86_64"
    ADDRESSMODEL="64"
    OSSL="mingw64"
elif [ $1 == "windows32-qt" ]; then
    MXE_TARGET="i686-w64-mingw32.static"
    MXE_TARGET1="i686-w64-mingw32.static"
    CPU_TARGET="i686"
    QT_BUILD="yes"
    HOST="x86"
    ADDRESSMODEL="32"
    OSSL="mingw"
elif [ $1 == "windows64-qt" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    MXE_TARGET1="x86_64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="yes"
    HOST="x86_64"
    ADDRESSMODEL="64"
    OSSL="mingw64"
else
    echo "Syntax: $0 [ windows32 | windows64 | windows32-qt | windows64-qt ]">&2
    exit 1
fi
# weird xenial travis bug
sudo systemctl enable mysql
sudo service mysql start
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
# sudo apt-get --yes install mxe-${MXE_TARGET}-miniupnpc
sudo apt-get --yes install mxe-${MXE_TARGET}-qttools
#sudo apt-get --yes install mxe-${MXE_TARGET}-db

# Some variables used by both Qt and daemon builds.
MXE_PATH=/usr/lib/mxe
export PATH=$PATH:$MXE_PATH/usr/bin
MXE_INCLUDE_PATH=$MXE_PATH/usr/${MXE_TARGET1}/include
MXE_LIB_PATH=$MXE_PATH/usr/${MXE_TARGET1}/lib
#TRAVIS_BUILD_DIR=~/Peepcoin

# Download, extract, build, install boost 1.65.1
wget https://sourceforge.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.bz2
tar -xjvf boost_1_58_0.tar.bz2 > /dev/null
cd boost_1_58_0
./bootstrap.sh --without-icu
echo "using gcc : mxe : $MXE_PATH/usr/bin/${MXE_TARGET1}-g++ : <rc>$MXE_PATH/usr/bin/${MXE_TARGET1}-windres <archiver>$MXE_PATH/usr/bin/${MXE_TARGET1}-ar <ranlib>$MXE_PATH/usr/bin/${MXE_TARGET1}-ranlib ;" > user-config.jam
export PATH=/usr/lib/mxe/usr/bin:$PATH
sudo ./b2 toolset=gcc address-model=${ADDRESSMODEL} target-os=windows variant=release threading=multi threadapi=win32 \
	link=static runtime-link=static --prefix=$MXE_PATH/usr/${MXE_TARGET1} --user-config=user-config.jam \
	--without-mpi --without-python -sNO_BZIP2=1 --layout=tagged install
cd ..

# Download, extract, build, install openssl1.0.2
wget https://www.openssl.org/source/old/1.0.2/openssl-1.0.2d.tar.gz
tar -xzvf openssl-1.0.2d.tar.gz > /dev/null
cp -R openssl-1.0.2d openssl-win32-build
cd openssl-win32-build
CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
RANLIB=$MXE_PATH/usr/bin/${MXE_TARGET1}-ranlib \
CROSS_COMPILE= ./Configure ${OSSL} no-asm no-shared --prefix=$MXE_PATH/usr/${MXE_TARGET1}
make > /dev/null
sudo make install
cd ..

# Download, extract, build miniupnp
wget -O miniupnpc-1.9.tar.gz http://miniupnp.free.fr/files/download.php?file=miniupnpc-1.9.tar.gz
tar zxvf miniupnpc-1.9.tar.gz
cd miniupnpc-1.9

CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
AR=$MXE_PATH/usr/bin/${MXE_TARGET1}-ar \
CFLAGS="-DSTATICLIB -I$MXE_PATH/usr/${MXE_TARGET1}/include" \
LDFLAGS="-L$MXE_PATH/usr/${MXE_TARGET1}/lib" \
make libminiupnpc.a

mkdir $MXE_PATH/usr/${MXE_TARGET1}/include/miniupnpc
cp *.h $MXE_PATH/usr/${MXE_TARGET1}/include/miniupnpc
cp libminiupnpc.a $MXE_PATH/usr/i686-w64-mingw32.static/lib

# Download, extract, build, install BDB4.8.30
cd ${TRAVIS_BUILD_DIR}
wget -N 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar zxf db-4.8.30.NC.tar.gz > /dev/null
cd db-4.8.30.NC
sed -i "s/WinIoCtl.h/winioctl.h/g" src/dbinc/win_db.h
mkdir build_mxe
cd build_mxe
make clean

CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
AR=$MXE_PATH/usr/bin/${MXE_TARGET1}-ar \
RANLIB=$MXE_PATH/usr/bin/${MXE_TARGET1}-ranlib \
CFLAGS="-DSTATICLIB -DDLL_EXPORT -DPIC -I$MXE_PATH/usr/${MXE_TARGET1}/include" \
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
tar xzf libpng-1.6.16.tar.gz > /dev/null
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
tar xzf qrencode-4.0.2.tar.gz > /dev/null
cd qrencode-4.0.2
make clean
LIBS="../libpng-1.6.16/.libs/libpng.a ../../mingw32/${MXE_TARGET1}/lib/libz.a" \
png_CFLAGS="-I../libpng-1.6.16" \
png_LIBS="-L../libpng-1.6.16/.libs" \
CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++ \
./configure \
    --enable-static \
    --disable-shared \
    --without-tools \
    --host=${HOST} \
    --prefix=$MXE_PATH/usr/${MXE_TARGET1}
make > /dev/null 2>&1
sudo make install > /dev/null 2>&1

# Parallel build, based on our number of CPUs available.
NCPU=`cat /proc/cpuinfo | grep -c ^processor`

# Invoke the magical commands to get Peepcoin built.
if [ $QT_BUILD == "no" ]; then
    cd ${TRAVIS_BUILD_DIR}
    cd src
    make -f makefile.linux-mingw -j $NCPU \
        DEPSDIR=$MXE_PATH/usr/$MXE_TARGET TARGET_PLATFORM=$CPU_TARGET > /dev/null 2>&1
else
    #sudo apt-get --yes install mxe-${MXE_TARGET}-qt5
    
	# clean Peepcoin directory before building leveldb because it will clean leveldb as well
	cd ${TRAVIS_BUILD_DIR}
	sudo make clean

	# cross-compile LevelDB
	cd ${TRAVIS_BUILD_DIR}
	cd src/leveldb
	chmod +x build_detect_platform
	sudo make clean
	TARGET_OS=OS_WINDOWS_CROSSCOMPILE make libleveldb.a libmemenv.a CC=$MXE_PATH/usr/bin/${MXE_TARGET1}-gcc CXX=$MXE_PATH/usr/bin/${MXE_TARGET1}-g++

	cd ${TRAVIS_BUILD_DIR}

	$MXE_PATH/usr/bin/${MXE_TARGET1}-qmake-qt5 \
		BOOST_LIB_SUFFIX=-mt-s \
		BOOST_THREAD_LIB_SUFFIX=_win32-mt-s \
		BOOST_INCLUDE_PATH=$MXE_INCLUDE_PATH/boost \
		BOOST_LIB_PATH=$MXE_LIB_PATH \
		OPENSSL_INCLUDE_PATH=$MXE_INCLUDE_PATH/openssl \
		OPENSSL_LIB_PATH=$MXE_LIB_PATH \
		BDB_INCLUDE_PATH=$MXE_INCLUDE_PATH \
		BDB_LIB_PATH=$MXE_LIB_PATH \
		MINIUPNPC_INCLUDE_PATH=$MXE_INCLUDE_PATH \
		MINIUPNPC_LIB_PATH=$MXE_LIB_PATH \
		QRENCODE_INCLUDE_PATH=$MXE_INCLUDE_PATH \
     		QRENCODE_LIB_PATH=$MXE_LIB_PATH \
		QMAKE_LRELEASE=$MXE_PATH/usr/$MXE_TARGET1/qt5/bin/lrelease Peepcoin-qt.pro

	#make clean
	make -j$(nproc) -f Makefile.Release
	mv release/peepcoin-qt.exe release/peepcoin-qt-${CPU_TARGET}.exe
	#make -j$(nproc) -f Makefile
fi

