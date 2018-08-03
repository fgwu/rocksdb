#!/bin/bash
script_dir=$(dirname $(readlink -f $0))
rocksdb_dir=$(dirname ${script_dir})

ks=8
vs=40
#: > k${ks}-binary.log
#: > k${ks}-hash.log
#for vs in 20 60 100 140; do
#for vs in 40; do
#    echo $vs
#    (vs=${vs} ks=${ks} block_index=binary ${script_dir}/db_bench_test.sh)
#    (vs=${vs} ks=${ks} block_index=hash ${script_dir}/db_bench_test.sh)
#done


for restart_interval in 1 16; do
    (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
       block_index=binary \
       ${script_dir}/db_bench_test.sh)

    for buck in 256 512 1024; do
        #for buck in 8192; do
        (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
           block_index=hash num_buckets=${buck} \
           ${script_dir}/db_bench_test.sh)
    done
done

:<<END
buck=8192
for threads in 10 20 30 40 50; do
    (vs=${vs} ks=${ks} block_index=binary threads=${threads} ${script_dir}/db_bench_test.sh)
    (vs=${vs} ks=${ks} block_index=hash num_buckets=${buck} threads=${threads} ${script_dir}/db_bench_test.sh)
done
END
