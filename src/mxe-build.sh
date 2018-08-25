#! /bin/bash

# Set our MXE and CPU information based on what type of
# wallet we want, and for which type of Windows.
if [ $1 == "windows32" ]; then
    MXE_TARGET="i686-w64-mingw32.static"
    CPU_TARGET="i686"
    QT_BUILD="no"
elif [ $1 == "windows64" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="no"
elif [ $1 == "windows32-qt" ]; then
    MXE_TARGET="i686-w64-mingw32.static"
    CPU_TARGET="i686"
    QT_BUILD="yes"
elif [ $1 == "windows64-qt" ]; then
    MXE_TARGET="x86-64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="yes"
else
    echo "Syntax: $0 [ windows32 | windows64 | windows32-qt | windows64-qt ]">&2
    exit 1
fi

# Add the MXE package repository.
sudo apt-get update

echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" \
    | sudo tee /etc/apt/sources.list.d/mxeapt.list
sudo apt-key adv --keyserver keyserver.ubuntu.com \
    --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

sudo apt-get update

# Add the required MXE build packages and libraries.
sudo apt-get --yes install mxe-${MXE_TARGET}-cc
sudo apt-get --yes install mxe-${MXE_TARGET}-openssl
#sudo apt-get --yes install mxe-${MXE_TARGET}-db
# Download, extract, build, install BDB4.8.30
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar zxvf db-4.8.30.NC.tar.gz

cd db-4.8.30.NC
MXE_PATH=/usr/lib/mxe
sudo sed -i "s/WinIoCtl.h/winioctl.h/g" src/dbinc/win_db.h
mkdir build_mxe
cd build_mxe

CC=$MXE_PATH/usr/bin/${MXE_TARGET}-gcc \
CXX=$MXE_PATH/usr/bin/${MXE_TARGET}-g++ \
../dist/configure \
    --disable-replication \
    --enable-mingw \
    --enable-cxx \
    --host x86 \
    --prefix=$MXE_PATH/usr/${MXE_TARGET}

make

sudo make install

sudo apt-get --yes install mxe-${MXE_TARGET}-boost
sudo apt-get --yes install mxe-${MXE_TARGET}-miniupnpc

# Parallel build, based on our number of CPUs available.
NCPU=`cat /proc/cpuinfo | grep -c ^processor`

# Some variables used by both Qt and daemon builds.
MXEDIR=/usr/lib/mxe
export PATH=$PATH:$MXEDIR/usr/bin
MXE_INCLUDE_PATH=/usr/lib/mxe/usr/${MXE_TARGET}/include
MXE_LIB_PATH=/usr/lib/mxe/usr/${MXE_TARGET}/lib

# Invoke the magical commands to get Peepcoin built.
if [ $QT_BUILD == "no" ]; then
    cd src
    make -f makefile.linux-mingw -j $NCPU \
        DEPSDIR=$MXEDIR/usr/$MXE_TARGET TARGET_PLATFORM=$CPU_TARGET
else
    sudo apt-get --yes install mxe-${MXE_TARGET}-qt5
    sudo apt-get --yes install mxe-${MXE_TARGET}-qttools
    MXE_PATH=/usr/lib/mxe
    export PATH=$MXE_PATH/usr/bin:$PATH

# pthread_t and pid_t change to u_int32_t in berkeley db mxe header files
sudo sed -i 's/pthread_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db.h
sudo sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db.h
sudo sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db_cxx.h

MXE_INCLUDE_PATH=$MXE_PATH/usr/i686-w64-mingw32.static/include
MXE_LIB_PATH=$MXE_PATH/usr/i686-w64-mingw32.static/lib

# cross-compile LevelDB
cd ..
cd ..
make clean
cd src/leveldb
make clean
TARGET_OS=NATIVE_WINDOWS make libleveldb.a libmemenv.a CC=/usr/lib/mxe/usr/bin/i686-w64-mingw32.static-gcc CXX=/usr/lib/mxe/usr/bin/i686-w64-mingw32.static-g++
cd ..
cd ..

i686-w64-mingw32.static-qmake-qt5 \
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
        QMAKE_LRELEASE=$MXE_PATH/usr/i686-w64-mingw32.static/qt5/bin/lrelease Peepcoin-qt.pro

#make clean
make -j$(nproc) -f Makefile.Release
#make -j$(nproc) -f Makefile
fi
