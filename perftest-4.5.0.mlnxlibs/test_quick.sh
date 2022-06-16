export ENABLE_MPRUD=1
#make -j 8
sleep 1.5
./ib_send_bw -d mlx5_1 -c UD 10.0.101.2 --report_gbits --tclass=106 -Q 1 $1
#./ib_send_bw -d mlx5_1 -c UD --report_gbits --tclass=106 -Q 1 $1
