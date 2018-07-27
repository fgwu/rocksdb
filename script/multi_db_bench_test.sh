#!/bin/bash
script_dir=$(dirname $(readlink -f $0))
rocksdb_dir=$(dirname ${script_dir})

ks=8
#: > k${ks}-binary.log
#: > k${ks}-hash.log
#for vs in 20 60 100 140; do
for vs in 40; do
    echo $vs
    (vs=${vs} ks=${ks} block_index=binary ${script_dir}/db_bench_test.sh)
    (vs=${vs} ks=${ks} block_index=hash ${script_dir}/db_bench_test.sh)
done
