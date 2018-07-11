// Copyright (c) 2011-present, Facebook, Inc. All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
#include <iostream>
#include <string>
#include <vector>

#include "rocksdb/slice.h"
#include "table/block_suffix_index.h"
#include "util/coding.h"
#include "util/hash.h"

namespace rocksdb {

const uint32_t kSeed = 2018;
const uint32_t kSeed_tag = 214; /* second hash seed */

inline uint16_t SuffixToBucket(const Slice& s, uint16_t num_buckets) {
  return (uint16_t)rocksdb::Hash(s.data(), s.size(), kSeed) % num_buckets;
}

void BlockSuffixIndexBuilder::Add(const Slice& key,
                                  const uint16_t& restart_index) {
  uint16_t idx = SuffixToBucket(key, num_buckets_);
  /* push a TAG to avoid false postive */
  /* the TAG is the hash function value of another seed */
  buckets_[idx].push_back((uint16_t)rocksdb::Hash(
                              key.data(), key.size(), kSeed_tag));
  buckets_[idx].push_back(restart_index);
  estimate_ += 2 * sizeof(uint16_t);
}

void BlockSuffixIndexBuilder::Finish(std::string& buffer) {
  // offset is the byte offset within the buffer
  std::vector<uint16_t> bucket_offsets(num_buckets_, 0);

  uint16_t map_start = static_cast<uint16_t>(buffer.size());

  // write each bucket to the string
  for (uint16_t i = 0; i < num_buckets_; i++) {
    // remember the start offset of the buckets in bucket_offsets
    bucket_offsets[i] = static_cast<uint16_t>(buffer.size());
    for (uint16_t elem : buckets_[i]) {
      // the elem is alternative "TAG" and "offset"
      PutFixed16(&buffer, elem);
    }
  }

  // write the bucket_offsets
  for (uint16_t i = 0; i < num_buckets_; i++) {
    PutFixed16(&buffer, bucket_offsets[i]);
  }

  // write NUM_BUCK
  PutFixed16(&buffer, num_buckets_);

  // write MAP_START
  PutFixed16(&buffer, map_start);
}

void BlockSuffixIndexBuilder::Reset() {
//  buckets_.clear();
  std::fill(buckets_.begin(), buckets_.end(), std::vector<uint16_t>());
  estimate_ = 0;
}

BlockSuffixIndex::BlockSuffixIndex(Slice block_content) {
  assert(block_content.size() >=
         2 * sizeof(uint16_t));  // NUM_BUCK and MAP_START

  data_ = block_content.data();
  size_ = static_cast<uint16_t>(block_content.size());

  map_start_ = data_ + DecodeFixed16(data_ + size_ - sizeof(uint16_t));
  assert(map_start_ < data_ + size_);

  num_buckets_ = DecodeFixed16(data_ + size_ - 2 * sizeof(uint16_t));
  assert(num_buckets_ > 0);

  assert(size_ >= sizeof(uint16_t) * (2 + num_buckets_));
  bucket_table_ = data_ + size_ - sizeof(uint16_t) * (2 + num_buckets_);

  assert(map_start_ <  bucket_table_);
}

void BlockSuffixIndex::Seek(Slice& key, std::vector<uint16_t>& bucket) const {
  assert(bucket.size() == 0);
  uint16_t idx = SuffixToBucket(key, num_buckets_);
  uint16_t bucket_off = DecodeFixed16(bucket_table_ + idx * sizeof(uint16_t));
  const char* limit;
  if (idx < num_buckets_ - 1) {
    // limited by the start offset of the next bucket
    limit = data_ +
            DecodeFixed16(bucket_table_ + (idx + 1) * sizeof(uint16_t));
  } else {
    // limited by the location of the NUM_BUCK
    limit = data_ + (size_ - 2 * sizeof(uint16_t));
  }

  uint16_t tag = rocksdb::Hash(key.data(), key.size(), kSeed_tag);
  for (const char* p = data_ + bucket_off; p < limit;
       p += 2 * sizeof(uint16_t)) {
    if (DecodeFixed16(p) == tag) {
      // only if the tag matches the string do we return the restart_index
      bucket.push_back(DecodeFixed16(p + sizeof(uint16_t)));
    }
  }
}

BlockSuffixIndexIterator* BlockSuffixIndex::NewIterator(
    const Slice& key) const {
  uint16_t idx = SuffixToBucket(key, num_buckets_);
  uint16_t bucket_off = DecodeFixed16(bucket_table_ + idx * sizeof(uint16_t));
  const char* limit;
  if (idx < num_buckets_ - 1) {
    // limited by the start offset of the next bucket
    limit = data_ +
            DecodeFixed16(bucket_table_ + (idx + 1) * sizeof(uint16_t));
  } else {
    // limited by the location of the NUM_BUCK
    limit = data_ + (size_ - 2 * sizeof(uint16_t));
  }

  uint16_t tag = rocksdb::Hash(key.data(), key.size(), kSeed_tag);

  return new BlockSuffixIndexIterator(data_ + bucket_off, limit, tag);
}

bool BlockSuffixIndexIterator::Valid() {
  return current_ < end_;
}

void BlockSuffixIndexIterator::Next() {
  for (current_ += 2 * sizeof(uint16_t); current_ < end_;
       current_ += 2 * sizeof(uint16_t)) {
    // stop at a offset that match the tag, i.e. a possible match
    if (DecodeFixed16(current_) == tag_) {
      break;
    }
  }
}

uint16_t BlockSuffixIndexIterator::Value() {
  return DecodeFixed16(current_+ sizeof(uint16_t));
}

}  // namespace rocksdb
