// Copyright (c) 2011-present, Facebook, Inc. All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
#include <string>
#include <vector>

#include "rocksdb/slice.h"
#include "table/data_block_hash_index.h"
#include "util/coding.h"
#include "util/hash.h"

namespace rocksdb {

const uint32_t kSeed = 2018;
const uint32_t kSeed_tag = 214; /* second hash seed */

inline uint16_t HashToBucket(const Slice& s, uint16_t num_buckets) {
  return static_cast<uint16_t>(
      rocksdb::Hash(s.data(), s.size(), kSeed) % num_buckets);
}

void DataBlockHashIndexBuilder::Add(const Slice& key,
                                    const uint8_t& restart_index) {
  uint16_t idx = HashToBucket(key, num_buckets_);
  /* push a TAG to avoid false postive */
  /* the TAG is the hash function value of another seed */
  uint8_t tag = static_cast<uint8_t>(
      rocksdb::Hash(key.data(), key.size(), kSeed_tag));
  buckets_[idx].push_back(HashTableEntry(tag, restart_index));
  estimate_ += sizeof(HashTableEntry);
}

void DataBlockHashIndexBuilder::Finish(std::string& buffer) {
  // offset is the byte offset within the buffer
  std::vector<uint16_t> bucket_offsets(num_buckets_, 0);

  uint16_t map_start = static_cast<uint16_t>(buffer.size());

  // write each bucket to the string
  for (uint16_t i = 0; i < num_buckets_; i++) {
    // remember the start offset of the buckets in bucket_offsets
    bucket_offsets[i] = static_cast<uint16_t>(buffer.size());
    for (HashTableEntry& entry : buckets_[i]) {
      buffer.append(const_cast<const char*>(
                        reinterpret_cast<char*>(&(entry.tag))),
                    sizeof(entry.tag));
      buffer.append(const_cast<const char*>(
                        reinterpret_cast<char*>(&(entry.restart_index))),
                    sizeof(entry.restart_index));
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

  // Because we use uint16_t address, we only support block less than 64KB
  assert(buffer.size() < (1 << 16));
}

void DataBlockHashIndexBuilder::Reset() {
  std::fill(buckets_.begin(), buckets_.end(), std::vector<HashTableEntry>());
  estimate_ = (num_buckets_ + 2) * sizeof(uint16_t);
}

DataBlockHashIndex::DataBlockHashIndex(Slice block_content) {
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

void DataBlockHashIndex::NewIterator(
    DataBlockHashIndexIterator* data_block_hash_iter, const Slice& key) const {
  assert(data_block_hash_iter);
  uint16_t idx = HashToBucket(key, num_buckets_);
  uint16_t bucket_off = DecodeFixed16(bucket_table_ + idx * sizeof(uint16_t));
  const char* limit;
  if (idx < num_buckets_ - 1) {
    // limited by the start offset of the next bucket
    limit = data_ + DecodeFixed16(bucket_table_ + (idx + 1) * sizeof(uint16_t));
  } else {
    // limited by the location of the beginning of the bucket index table.
    limit = data_ + (size_ - 2 * sizeof(uint16_t) -
                     num_buckets_ * sizeof(int16_t));
  }
  uint8_t tag = (uint8_t)rocksdb::Hash(key.data(), key.size(), kSeed_tag);
  data_block_hash_iter->Initialize(data_ + bucket_off, limit, tag);
}

void DataBlockHashIndexIterator::Initialize(const char* start, const char* end,
                                            const uint8_t tag) {
  end_ = end;
  tag_ = tag;
  current_ = start - sizeof(HashTableEntry);
  Next();
}

bool DataBlockHashIndexIterator::Valid() {
  return current_ < end_;
}

void DataBlockHashIndexIterator::Next() {
  for (current_ += sizeof(HashTableEntry); current_ < end_;
       current_ += sizeof(HashTableEntry)) {
    // stop at a offset that match the tag, i.e. a possible match
    uint8_t tag_found = static_cast<uint8_t>(*(current_));
    if (tag_found == tag_) {
      break;
    }
  }
}

uint8_t DataBlockHashIndexIterator::Value() {
  return static_cast<uint8_t>(*(current_ + sizeof(HashTableEntry::tag)));
}

}  // namespace rocksdb
