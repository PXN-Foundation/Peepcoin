Peepcoin-qt: Qt5 GUI for Peepcoin
===============================

Build instructions
===================

Debian (Not up to date)
-------

First, make sure that the required packages for Qt5 development of your
distribution are installed, for Debian and Ubuntu these are:

::

    apt-get install qt5-default qt5-qmake qtbase5-dev-tools qttools5-dev-tools \
        build-essential libboost-dev libboost-system-dev \
        libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev \
        libssl-dev libdb++-dev

then execute the following:

::

    qmake
    make

Alternatively, install Qt Creator and open the `peepcoin-qt.pro` file.

An executable named `peepcoin-qt` will be built.


Windows
--------

Windows build instructions:

See build-msw.txt


Mac OS X
--------
Homebrew (current updated build steps)
- Install Homebrew - http://brew.sh
/usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"

- Accept Xcode License and allow it to install command line tools

-Install Git
brew install git

- Install Berkley DB 4.8
brew install berkeley-db4

- Install OpenSSL
brew install openssl

- Install Boost
brew uninstall boost (ONLY if Boost is already installed) - 
brew install boost@1.60
brew link boost@1.60 -force
(also testing with boost@1.57 for wallet.dat issue)

- Install QT
brew install qt5
brew link qt5 -force

- Install QREncode
brew install libqrencode

- Install WGet
brew install wget

- Download, compile, install Miniupnpc
wget -O miniupnpc.tar.gz http://miniupnp.free.fr/files/download.php?file=miniupnpc-1.9.20150206.tar.gz
tar xvzf miniupnpc.tar.gz
mv miniupnpc-1.9.20150206 miniupnpc
cd miniupnpc
make -f Makefile upnpc-static
sudo INSTALLPREFIX=/usr/local make install

- Clone DAPSCoin GitHub
https://github.com/DAPSCoin/PeepCoin-TheCoinForThePeople.git

- Change to PeepCoin-TheCoinForThePeople directory
cd PeepCoin-TheCoinForThePeople

Build Peepcoin-Qt
qmake RELEASE=1 USE_QRCODE=1
make

-Install EasyInstll and AppScript
sudo easy_install appscript

To deploy .dmg:
~/PeepCoin-TheCoinForThePeople/contrib/macdeploy/macdeployqtplus peepcoin-Qt.app -add-qt-tr da,de,es,hu,ru,uk,zh_CN,zh_TW -dmg -fancy ~/PeepCoin-TheCoinForThePeople/contrib/macdeploy/fancy.plist -verbose 2


Mac Ports (not up to date)

- Download and install the `Qt Mac OS X SDK`_. It is recommended to also install Apple's Xcode with UNIX tools.

- Download and install `MacPorts`_.

- Execute the following commands in a terminal to get the dependencies:

::

	sudo port selfupdate
	sudo port install boost db48 miniupnpc

- Open the .pro file in Qt Creator and build as normal (cmd-B)

.. _`Qt Mac OS X SDK`: http://qt-project.org/downloads
.. _`MacPorts`: http://www.macports.org/install.php


Build configuration options
============================

UPNnP port forwarding
---------------------

To use UPnP for port forwarding behind a NAT router (recommended, as more connections overall allow for a faster and more stable peepcoin experience), pass the following argument to qmake:

::

    qmake "USE_UPNP=1"

(in **Qt Creator**, you can find the setting for additional qmake arguments under "Projects" -> "Build Settings" -> "Build Steps", then click "Details" next to **qmake**)

This requires miniupnpc for UPnP port mapping.  It can be downloaded from
http://miniupnp.tuxfamily.org/files/.  UPnP support is not compiled in by default.

Set USE_UPNP to a different value to control this:

+------------+--------------------------------------------------------------------------+
| USE_UPNP=- | no UPnP support, miniupnpc not required;                                 |
+------------+--------------------------------------------------------------------------+
| USE_UPNP=0 | (the default) built with UPnP, support turned off by default at runtime; |
+------------+--------------------------------------------------------------------------+
| USE_UPNP=1 | build with UPnP support turned on by default at runtime.                 |
+------------+--------------------------------------------------------------------------+

Notification support for recent (k)ubuntu versions
---------------------------------------------------

To see desktop notifications on (k)ubuntu versions starting from 10.04, enable usage of the
FreeDesktop notification interface through DBUS using the following qmake option:

::

    qmake "USE_DBUS=1"

Generation of QR codes
-----------------------

libqrencode may be used to generate QRCode images for payment requests. 
It can be downloaded from http://fukuchi.org/works/qrencode/index.html.en, or installed via your package manager. Pass the USE_QRCODE 
flag to qmake to control this:

+--------------+--------------------------------------------------------------------------+
| USE_QRCODE=0 | (the default) No QRCode support - libarcode not required                 |
+--------------+--------------------------------------------------------------------------+
| USE_QRCODE=1 | QRCode support enabled                                                   |
+--------------+--------------------------------------------------------------------------+


Berkely DB version warning
==========================

A warning for people using the *static binary* version of Peepcoin on a Linux/UNIX-ish system (tl;dr: **Berkely DB databases are not forward compatible**).

The static binary version of Peepcoin is linked against libdb 5.0 (see also `this Debian issue`_).

Now the nasty thing is that databases from 5.X are not compatible with 4.X.

If the globally installed development package of Berkely DB installed on your system is 5.X, any source you
build yourself will be linked against that. The first time you run with a 5.X version the database will be upgraded,
and 4.X cannot open the new format. This means that you cannot go back to the old statically linked version without
significant hassle!

.. _`this Debian issue`: http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=621425

Ubuntu 11.10 warning
====================

Ubuntu 11.10 has a package called 'qt-at-spi' installed by default.  At the time of writing, having that package
installed causes peepcoin-qt to crash intermittently.  The issue has been reported as `launchpad bug 857790`_, but
isn't yet fixed.

Until the bug is fixed, you can remove the qt-at-spi package to work around the problem, though this will presumably
disable screen reader functionality for Qt apps:

::

    sudo apt-get remove qt-at-spi

.. _`launchpad bug 857790`: https://bugs.launchpad.net/ubuntu/+source/qt-at-spi/+bug/857790
