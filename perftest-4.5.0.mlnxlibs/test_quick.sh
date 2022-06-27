#make -j 8
sleep 1.5

./ib_send_bw -F -d mlx5_1 -c RC 10.0.101.2 --report_gbits --tclass=106 -Q 1 $1
#./ib_send_bw -F -d mlx5_1 -c RC --report_gbits --tclass=106 -Q 1 $1

# Execute with GDB
# gdb -ex run --args ./ib_send_bw -F -d mlx5_1 -c RC --report_gbits --tclass=106 -Q 1 $1
