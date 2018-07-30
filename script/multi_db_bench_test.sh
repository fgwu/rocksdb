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

(vs=${vs} ks=${ks} block_index=binary ${script_dir}/db_bench_test.sh)
for buck in 1 2 4 8 16 32 64 128 256 512; do
#for buck in 1 512; do
 #   echo vs=${vs} ks=${ks} block_index=hash num_buckets=${buck}
    (vs=${vs} ks=${ks} block_index=hash num_buckets=${buck} ${script_dir}/db_bench_test.sh)
done
