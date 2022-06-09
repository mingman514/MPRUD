mlnx_qos -i enp1s0f1 --pfc 0,0,0,1,0,0,0,0
sleep 2
mlnx_qos -i enp1s0f1 --buffer_size 30848,230912,0,0,0,0,0,0
sleep 2
mlnx_qos -i enp1s0f1 --trust=dscp
sleep 2
mlnx_tune -p HIGH_THROUGHPUT
sleep 2
cma_roce_tos -d mlx5_1 -t 106
sleep 2
sysctl -w net.ipv4.tcp_ecn=1
sleep 2
echo 106 > /sys/class/infiniband/mlx5_1/tc/1/traffic_class
cat /sys/class/infiniband/mlx5_1/tc/1/traffic_class
