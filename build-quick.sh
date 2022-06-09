set -e
cd libibverbs-41mlnx1/
make -j 8 && sudo make install
sleep 1

cd ../libmlx5-41mlnx1/
make -j 8 && sudo make install
sleep 1

#cd ../libmlx4-41mlnx1/
#./build.sh
#sleep 2

cd ../perftest-4.5.0.mlnxlibs/
make -j 8
cd ..

echo 
echo ">>> QUICK BUILD COMPLETE <<<"
