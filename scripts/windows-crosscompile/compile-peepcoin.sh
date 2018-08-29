#!/bin/bash -ev

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
    MXE_TARGET="x86_64-w64-mingw32.static"
    CPU_TARGET="x86_64"
    QT_BUILD="yes"
else
    echo "Syntax: $0 [ windows32 | windows64 | windows32-qt | windows64-qt ]">&2
    exit 1
fi

MXE_PATH=/usr/lib/mxe
export PATH=$MXE_PATH/usr/bin:$PATH

# pthread_t and pid_t change to u_int32_t in berkeley db mxe header files
sed -i 's/pthread_t/u_int32_t/g' /usr/lib/mxe/usr/${MXE_TARGET}/include/db.h
sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/${MXE_TARGET}/include/db.h
sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/${MXE_TARGET}/include/db_cxx.h

cd src/leveldb
TARGET_OS=NATIVE_WINDOWS make libleveldb.a libmemenv.a CC=/usr/lib/mxe/usr/bin/${MXE_TARGET}-gcc CXX=/usr/lib/mxe/usr/bin/${MXE_TARGET}-g++
cd ..
cd ..

MXE_INCLUDE_PATH=$MXE_PATH/usr/${MXE_TARGET}/include
MXE_LIB_PATH=$MXE_PATH/usr/${MXE_TARGET}/lib

${MXE_TARGET}-qmake-qt5 \
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
        QMAKE_LRELEASE=$MXE_PATH/usr/${MXE_TARGET}/qt5/bin/lrelease Peepcoin-qt.pro

#make clean
make -j$(nproc) -f Makefile.Release
#make -j$(nproc) -f Makefile