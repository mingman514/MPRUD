# P2P
sudo ifconfig enp1s0f0 192.168.0.2/24 mtu 4200 up

# Switch
sudo modprobe 8021q
sudo ifconfig enp1s0f1 mtu 4200 up
sudo ip link add link enp1s0f1 name enp1s0f1.201 type vlan id 201
sudo ip link set dev enp1s0f1.201 mtu 4200 up
sudo ip addr add 10.0.201.2/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.3/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.4/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.5/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.6/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.7/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.8/24 dev enp1s0f1.201
sudo ip addr add 10.0.201.9/24 dev enp1s0f1.201
sudo ip route add 10.0.0.0/16 via 10.0.201.1
