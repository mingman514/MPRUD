make clean
./autogen.sh
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
make -j 8 CFLAGS=-Wno-error
