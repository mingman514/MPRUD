make -j 8
sleep 1.5
./ib_send_bw -d mlx5_1 -c UD 10.0.101.2 --report_gbits -x 5 $1
#./ib_send_bw -d mlx5_1 -c UD -x 5 --report_gbits $1
