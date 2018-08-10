#!/bin/bash
script_dir=$(dirname $(readlink -f $0))
rocksdb_dir=$(dirname ${script_dir})

ks=8
vs=40
buck=500
#: > k${ks}-binary.log
#: > k${ks}-hash.log
#for vs in 20 60 100 140; do
#for vs in 40; do
#    echo $vs
#    (vs=${vs} ks=${ks} block_index=binary ${script_dir}/db_bench_test.sh)
#    (vs=${vs} ks=${ks} block_index=hash ${script_dir}/db_bench_test.sh)
#done

:<<END

for restart_interval in 1; do
    (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
       block_index=binary \
       ${script_dir}/db_bench_test.sh)
    #    for buck in 256 512 1024 2048; do
    for buck in 512; do
        #for buck in 8192; do
        (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
           block_index=hash num_buckets=${buck} \
           ${script_dir}/db_bench_test.sh)

    done
done
END

:<<END
buck=8192
for threads in 10 20 30 40 50; do
    (vs=${vs} ks=${ks} block_index=binary threads=${threads} ${script_dir}/db_bench_test.sh)
    (vs=${vs} ks=${ks} block_index=hash num_buckets=${buck} threads=${threads} ${script_dir}/db_bench_test.sh)
done
END

:<<END
#RI and util ratio
for restart_interval in 1 4 16; do
    (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
       block_index=binary \
       ${script_dir}/db_bench_test.sh)
    for util_ratio in 1 0.9 0.8 0.7 0.6 0.5 0.4 0.3 0.2 0.1; do
        (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
           block_index=hash num_buckets=${buck} util_ratio=${util_ratio} \
           ${script_dir}/db_bench_test.sh)
    done
done
END

:<<END
# thread number
restart_interval=16
for threads in 1 4 16; do
    (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
       block_index=binary threads=${threads}\
       ${script_dir}/db_bench_test.sh)
    for util_ratio in 0.8; do
        (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
           block_index=hash num_buckets=${buck} util_ratio=${util_ratio} \
           threads=${threads} \
           ${script_dir}/db_bench_test.sh)
    done
done
END

:<<END
# ks
for ks in 10 20 30 40 50; do
    (ks=${ks} block_index=binary \
       ${script_dir}/db_bench_test.sh)
    for util_ratio in 0.8; do
        (ks=${ks} block_index=hash num_buckets=${buck} util_ratio=${util_ratio} \
           ${script_dir}/db_bench_test.sh)
    done
done

# vs
for vs in 20 40 60 80 100; do
    (vs=${vs} block_index=binary \
       ${script_dir}/db_bench_test.sh)
    for util_ratio in 0.8; do
        (vs=${vs} block_index=hash num_buckets=${buck} util_ratio=${util_ratio} \
           ${script_dir}/db_bench_test.sh)
    done
done
END


# cache
restart_interval=1
threads=16
for cache_size in 1000000000; do
    (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
       block_index=binary \
       threads=${threads} \
       cache_size=${cache_size} \
       ${script_dir}/db_bench_test.sh)
    for util_ratio in 1 0.9 0.8 0.7 0.6 0.5; do
        (vs=${vs} ks=${ks} restart_interval=${restart_interval} \
           block_index=hash num_buckets=${buck} util_ratio=${util_ratio} \
           threads=${threads} \
           cache_size=${cache_size} \
           ${script_dir}/db_bench_test.sh)
    done
done
