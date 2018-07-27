cd rocksdb
echo "Load 1B keys sequentially into database....."
bpl=10485760;mcz=2;del=300000000;levels=6;ctrig=4; delay=8; stop=12; wbn=3; \
    mbc=20; mb=67108864;wbs=134217728; sync=0; r=1000000; t=1; vs=800; \
    bs=4096; cs=1048576; of=500000; si=1000000; \
    ./db_bench \
        --benchmarks=fillseq --disable_seek_compaction=1 --mmap_read=0 \
        --statistics=1 --histogram=1 --num=$r --threads=$t --value_size=$vs \
        --block_size=$bs --cache_size=$cs --bloom_bits=10 --cache_numshardbits=6 \
        --open_files=$of --verify_checksum=1 --sync=$sync --disable_wal=1 \
        --compression_type=none --stats_interval=$si --compression_ratio=0.5 \
        --write_buffer_size=$wbs --target_file_size_base=$mb \
        --max_write_buffer_number=$wbn --max_background_compactions=$mbc \
        --level0_file_num_compaction_trigger=$ctrig \
        --level0_slowdown_writes_trigger=$delay \
        --level0_stop_writes_trigger=$stop --num_levels=$levels \
        --delete_obsolete_files_period_micros=$del --min_level_to_compress=$mcz \
        --stats_per_interval=1 --max_bytes_for_level_base=$bpl \
          --use_existing_db=0
