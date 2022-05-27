echo 
echo Start Building libibverbs

## remove shared object
#cd ./src
#sudo rm -f /usr/lib/libmprud.so
#rm -f mprud.o
#
## create so
#g++ -c -g -O3 -fPIC mprud.c
#g++ -g -shared -o libmprud.so mprud.o
#
## copy so
#sudo cp libmprud.so /usr/lib/libmprud.so
#cd ..

sleep 2
make clean
./autogen.sh
sleep 2
./configure --prefix=/usr/ --libdir=/usr/lib/ --sysconfdir=/etc/
sleep 2
make -j 8 CFLAGS=-Wno-error
sleep 2
sudo make install
