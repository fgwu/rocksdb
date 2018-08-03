// Copyright (c) 2011-present, Facebook, Inc. All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <string>
#include <vector>

#include "rocksdb/slice.h"

namespace rocksdb {
// This is an experimental feature aiming to reduce the CPU utilization of
// point-lookup within a data-block. It is not used in per-table index-blocks.
// It supports Get(), but not Seek() or Scan(). If the key does not exist,
// the iterator is set to invalid.
//
// A serialized hash index is appended to the data-block. The new block data
// format is as follows:
//
// DATA_BLOCK: [RI RI RI ... RI RI_IDX HASH_IDX FOOTER]
//
// RI:       Restart Interval (the same as the default data-block format)
// RI_IDX:   Restart Interval index (the same as the default data-block format)
// HASH_IDX: The new data-block hash index feature.
// FOOTER:   A 32bit block footer, which is the NUM_RESTARTS with the MSB as
//           the flag indicating if this hash index is in use. Note that
//           given a data block < 32KB, the MSB is never used. So we can
//           borrow the MSB as the hash index flag. Therefore, this format is
//           compatible with the legacy data-blocks with num_restarts < 32768,
//           as the MSB is 0.
//
// The format of the data-block hash index is as follows:
//
// HASH_IDX: [B B B ... B NUM_BUCK MAP_START]
//
// B:         bucket, an array of restart index. Each buckets is uint8_t.
// NUM_BUCK:  Number of buckets, which is the length of the bucket array.
// MAP_START: the starting offset of the data-block hash index.
//
// We reserve two special flag:
//    kNoEntry=255,
//    kCollision=254.
//
// Buckets are initialized to be kNoEntry.
//
// When storing a key in the hash index, the key is first hashed to a bucket.
// If there the bucket is empty (kNoEntry), the restart index is stored in
// the bucket. If there is already a restart index there, we will update the
// existing restart index to a collision marker (kCollision). If the
// the bucket is already marked as collision, we do not store the restart
// index either.
//
// During query process, a key is first hashed to a bucket. Then we examine if
// the buckets store nothing (kNoEntry) or the bucket had a collision
// (kCollision). If either of those happens, we get the restart index of
// the key and will directly go to the restart interval to search the key.
//
// Note that we only support blocks with #restart_interval < 254. If a block
// has more restart interval than that, hash index will not be create for it.

const uint8_t kNoEntry = 255;
const uint8_t kCollision = 254;

class DataBlockHashIndexBuilder {
 public:
  explicit DataBlockHashIndexBuilder(uint16_t n)
      : num_buckets_(n),
        buckets_(n, kNoEntry),
        estimate_(2 * sizeof(uint16_t) /*num_buck and maps_start*/ +
                  n * sizeof(uint8_t) /*n buckets*/) {}
  void Add(const Slice& key, const uint8_t& restart_index);
  void Finish(std::string& buffer);
  void Reset();
  inline size_t EstimateSize() { return estimate_; }

 private:
  uint16_t num_buckets_;
  std::vector<uint8_t> buckets_;
  size_t estimate_;
};

class DataBlockHashIndex {
 public:
  DataBlockHashIndex():size_(0), num_buckets_(0) {}

  void Initialize(Slice block_content);
  inline uint16_t DataBlockHashMapStart() const {
    return static_cast<uint16_t>(map_start_ - data_);
  }

  uint8_t Seek(const Slice& key) const;
  bool Valid() const {return size_ != 0;}

 private:
  const char *data_;
  // To make the serialized hash index compact and to save the space overhead,
  // here all the data fields persisted in the block are in uint16 format.
  // We find that a uint16 is large enough to index every offset of a 64KiB
  // block.
  // So in other words, DataBlockHashIndex does not support block size equal
  // or greater then 64KiB.
  uint16_t size_;
  uint16_t num_buckets_;
  const char *map_start_;    // start of the map
  const char *bucket_table_; // start offset of the bucket index table
};

}  // namespace rocksdb
