#!/bin/bash

: ${ks:=16}
: ${vs:=32}
: ${block_index:=binary}
: ${num_buckets:=500}
: ${num:=20000000}
: ${reads:=1000000}
: ${threads:=1}
: ${restart_interval:=16}

#test_prefix=k${ks}v${vs}t${threads}_${block_index}${num_buckets}

script_dir=$(dirname $(readlink -f $0))
rocksdb_dir=$(dirname ${script_dir})

DB_BENCH=${rocksdb_dir}/db_bench

db_dir=/home/fwu/db


dataset_prefix=k${ks}_v${vs}_N$(($num/1000000))
index_prefix=RI${restart_interval}_${block_index}${num_buckets}
read_prefix=R$(($reads/1000000))_t${threads}


db=${db_dir}/${dataset_prefix}/${index_prefix}

if [ ! -d ${db} ]; then
    mkdir -p ${db}
fi

log_dir=${dataset_prefix}

if [ ! -d ${log_dir} ]; then
    mkdir -p ${log_dir}
fi

fig_dir=/mnt/public/fwu/${dataset_prefix}

if [ ! -d ${fig_dir} ]; then
    mkdir -p ${fig_dir}
fi

test_prefix=${read_prefix}_${index_prefix}
write_log=${log_dir}/fillseq_${test_prefix}.log
read_log=${log_dir}/readrand_${test_prefix}.log
fig_name=${test_prefix}_$(date +"%Y-%m-%d_%H-%M-%S").svg
perf_figure=${fig_dir}/${fig_name}
echo ---------------------------------------------------------------------
echo test:" "${dataset_prefix}_${test_prefix}
echo R_log:""$(readlink -f ${read_log})
echo W_log:""$(readlink -f ${write_log})
#echo fig_dir:""${perf_figure}
echo fig:"  "https://home.fburl.com/~fwu/${fig_name}
echo ---------------------------------------------------------------------

$DB_BENCH  --data_block_index_type=${block_index} \
           --db=${db} \
           --block_size=16000 --level_compaction_dynamic_level_bytes=1 \
           --num=$num \
           --key_size=$ks \
           --value_size=$vs \
           --benchmarks=fillseq --compression_type=snappy \
           --statistics=false --block_restart_interval=1 \
           --compression_ratio=0.4 \
           --block_hash_num_buckets=${num_buckets}\
           --statistics=true \
           --perf_level=2 \
           >${write_log}


time perf record -g -F 99 \
$DB_BENCH  --data_block_index_type=${block_index} \
           --db=${db} \
           --block_size=16000 --level_compaction_dynamic_level_bytes=1 \
           --use_existing_db=true \
           --num=${num} \
           --reads=${reads} \
           --key_size=$ks \
           --value_size=$vs \
           --benchmarks=readtocache,readrandom \
           --compression_type=snappy \
           --block_restart_interval=16 \
           --compression_ratio=0.4 \
           --cache_size=20000000000 \
           --block_hash_num_buckets=${num_buckets} \
           --use_direct_reads \
           --disable_auto_compactions \
           --threads=${threads} \
           --statistics=true \
           --perf_level=2 \
           > ${read_log}

#           --read_cache_size=200000000



perf script | stackcollapse-perf.pl > out.perf-folded
flamegraph.pl out.perf-folded > ${perf_figure}

micros_op=$(grep readrandom ${read_log} | awk '{print $3}')
ops_sec=$(grep readrandom ${read_log} | awk '{print $5}')
bw=$(grep readrandom ${read_log} | awk '{print $7}')
cpu_util=$(grep -Po "<title>rocksdb::DataBlockIter::Seek.*samples, \K[0-9]+\.[^%]+" ${perf_figure} | awk '{sum += $1} END {print sum}')
space=$(du ${db} | grep -Po '^\d+')


hash_fallback=$(grep hash.index.fallback ${read_log} | awk '{print $4}')
hash_success=$(grep hash.index.success ${read_log} | awk '{print $4}')
hash_total=$((${hash_fallback} + ${hash_success}))
fallback_ratio=$(python -c "print(1.0*${hash_fallback}/${hash_total})")
echo ${dataset_prefix}_${read_prefix}_${index_prefix}, \
     ${bw}, ${cpu_util}, ${space}, ${hash_fallback}, ${hash_total}, \
     ${fallback_ratio} | tee -a ${script_dir}/k.log

:<<END
echo ${dataset_prefix}_${read_prefix}_${index_prefix}, ${micros_op}, ${ops_sec}, \
     ${bw}, ${cpu_util}, ${space}, ${hash_fallback}, ${hash_total}, \
     ${fallback_ratio} | tee -a ${script_dir}/k.log
END
