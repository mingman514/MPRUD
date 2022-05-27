set -e
cd libibverbs-41mlnx1/
./build.sh
sleep 2

cd ../libmlx5-41mlnx1/
./build.sh
sleep 2

#cd ../libmlx4-41mlnx1/
#./build.sh
#sleep 2

cd ../perftest-4.5.0.mlnxlibs/
./build.sh

echo 
echo BUILD COMPLETE
