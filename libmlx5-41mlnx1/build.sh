echo 
echo Start Building libmlx5-41mlnx1
sleep 2
make clean
./autogen.sh
sleep 2
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
sleep 2
make -j 8 CFLAGS=-Wno-error
sleep 2
sudo make install
