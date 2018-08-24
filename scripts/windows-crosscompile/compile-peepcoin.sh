#!/bin/bash -ev


MXE_PATH=/usr/lib/mxe
export PATH=$MXE_PATH/usr/bin:$PATH

# pthread_t and pid_t change to u_int32_t in berkeley db mxe header files
sed -i 's/pthread_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db.h
sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db.h
sed -i 's/pid_t/u_int32_t/g' /usr/lib/mxe/usr/i686-w64-mingw32.static/include/db_cxx.h

MXE_INCLUDE_PATH=$MXE_PATH/usr/i686-w64-mingw32.static/include
MXE_LIB_PATH=$MXE_PATH/usr/i686-w64-mingw32.static/lib

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

