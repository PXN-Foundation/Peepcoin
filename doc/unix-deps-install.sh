sudo add-apt-repository ppa:bitcoin/bitcoin -y
sudo apt update

sudo apt install git build-essential libdb4.8-dev libdb4.8++-dev -y
sudo apt install libssl-dev libboost-all-dev libqrencode-dev libminiupnpc-dev -y
sudo apt install qt5-default qt5-qmake qtbase5-dev-tools qttools5-dev-tools -y

sudo apt upgrade -y

wget http://download.sourceforge.net/libpng/libpng-1.6.16.tar.gz
tar xvzf libpng-1.6.16.tar.gz
cd libpng-1.6.16
./configure --disable-shared
make
cp .libs/libpng16.a .libs/libpng.a
LIBS="../libpng-1.6.16/.libs/libpng.a"
png_CFLAGS="-I../libpng-1.6.16"
png_LIBS="-L../libpng-1.6.16/.libs"

cd ..

wget http://fukuchi.org/works/qrencode/qrencode-3.4.4.tar.gz
tar xvzf qrencode-3.4.4.tar.gz
cd qrencode-3.4.4
./configure --enable-static --disable-shared --without-tools
make
sudo make install