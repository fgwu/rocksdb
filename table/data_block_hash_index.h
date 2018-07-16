// Copyright (c) 2011-present, Facebook, Inc. All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <string>
#include <vector>

namespace rocksdb {

// The format of the datablock hash map is as follows:
//
// B B B ... B IDX NUM_BUCK MAP_START
//
// NUM_BUCK: Number of buckets, which is the length of the IDX array.
//
// IDX:      Array of offsets, each pointing to the starting offset (relative to
//           MAP_START) of one hash bucket.
//
// B:        B = bucket, an array consisting of a list of restart index, and the
//           second hash tag value as tagBucket, an array consisting of a list
//           of restart interval offsets.
//           We do not have to store the length of individual buckets, as they
//           are delimited by the next bucket offset.
//
// MAP_START: the start offset of the datablock hash map.
//
// Each bucket B has the following structure:
// [TAG RESTART_INDEX][TAG RESTART_INDEX]...[TAG RESTART_INDEX]
//
// The datablock hash map is construct right in-place of the block without any
// data been copied.

class DataBlockHashIndexBuilder {
 public:
  DataBlockHashIndexBuilder(uint32_t n) :
      num_buckets_(n),
      buckets_(n),
      estimate_((n + 2) * sizeof(uint32_t) /* n buckets, 2 num at the end */){}
  void Add(const Slice &key, const uint32_t &restart_index);
  void Finish(std::string& buffer);
  void Reset();
  inline size_t EstimateSize() { return estimate_; }

 private:
  uint32_t num_buckets_;
  std::vector<std::vector<uint32_t>> buckets_;
  size_t estimate_;
};

class DataBlockHashIndexIterator;

class DataBlockHashIndex {
 public:
  DataBlockHashIndex(Slice  block_content);
  void Seek(const Slice& key, std::vector<uint32_t>& bucket) const;

  inline uint32_t DataBlockHashMapStart() const {
    return static_cast<uint32_t>(map_start_ - data_);
  }

  DataBlockHashIndexIterator* NewIterator(const Slice& key) const;

 private:
  const char *data_;
  uint32_t size_;
  uint32_t num_buckets_;
  const char *map_start_;    // start of the map
  const char *bucket_table_; // start offset of the bucket index table
};

class DataBlockHashIndexIterator {
 public:
  DataBlockHashIndexIterator(const char* start, const char* end,
                           const uint32_t tag):
      start_(start), end_(end), tag_(tag) {
    current_ = start - 2 * sizeof(uint32_t);
    Next();
  }
  bool Valid();
  void Next();
  uint32_t Value();
 private:
  const char* start_; // [start_,  end_) defines the bucket range
  const char* end_;
  const uint32_t tag_; // the fingerprint (2nd hash value) of the searching key
  const char* current_;
};

}  // namespace rocksdb
