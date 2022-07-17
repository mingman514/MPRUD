sleep 2

./ib_send_bw -F -d mlx5_1 -Q 1 --report_gbits 10.0.101.2 --run_infinitely  -x 7 -s 1048576 -p 10001  > spine1 &
./ib_send_bw -F -d mlx5_1 -Q 1 --report_gbits 10.0.101.2 --run_infinitely  -x 9 -s 1048576 -p 10002 > spine2 &
./ib_send_bw -F -d mlx5_1 -Q 1 --report_gbits 10.0.101.2 --run_infinitely  -x 11 -s 1048576 -p 10003 > spine3 &
./ib_send_bw -F -d mlx5_1 -Q 1 --report_gbits 10.0.101.2 --run_infinitely  -x 5 -s 1048576 -p 10004 > spine4 &
