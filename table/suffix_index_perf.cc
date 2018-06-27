//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
#include <stdio.h>
#include <algorithm>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "db/dbformat.h"
#include "db/write_batch_internal.h"
#include "db/memtable.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/iterator.h"
#include "rocksdb/table.h"
#include "rocksdb/slice_transform.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/format.h"
#include "util/random.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

static std::string RandomString(Random* rnd, int len) {
  std::string r;
  test::RandomString(rnd, len, &r);
  return r;
}
std::string GenerateKey(int primary_key, int secondary_key, int padding_size,
                        Random *rnd) {
  char buf[500];
  char *p = &buf[0];
  snprintf(buf, sizeof(buf), "%6d%4d", primary_key, secondary_key);
  std::string k(p);
  if (padding_size) {
    k += RandomString(rnd, padding_size);
  }

  return k;
}

// Generate random key value pairs.
// The generated key will be sorted. You can tune the parameters to generated
// different kinds of test key/value pairs for different scenario.
void GenerateRandomKVs(std::vector<std::string> *keys,
                       std::vector<std::string> *values, const int from,
                       const int len, const int step = 1,
                       const int padding_size = 0,
                       const int keys_share_prefix = 1) {
  Random rnd(302);

  // generate different prefix
  for (int i = from; i < from + len; i += step) {
    // generating keys that shares the prefix
    for (int j = 0; j < keys_share_prefix; ++j) {
      keys->emplace_back(GenerateKey(i, j, padding_size, &rnd));

      // 100 bytes values
      values->emplace_back(RandomString(&rnd, 100));
    }
  }
}
// return the block contents
BlockContents GetBlockContents(std::unique_ptr<BlockBuilder> *builder,
                               const std::vector<std::string> &keys,
                               const std::vector<std::string> &values,
                               const int /*prefix_group_size*/ = 1) {
  builder->reset(new BlockBuilder(1 /* restart interval */));

  // Add only half of the keys
  for (size_t i = 0; i < keys.size(); ++i) {
    (*builder)->Add(keys[i], values[i]);
  }
  Slice rawblock = (*builder)->Finish();

  BlockContents contents;
  contents.data = rawblock;
  contents.cachable = false;

  return contents;
}

void CheckBlockContents(BlockContents contents, const int max_key,
                        const std::vector<std::string> &keys,
                        const std::vector<std::string> &values) {
  const size_t prefix_size = 6;
  // create block reader
  BlockContents contents_ref(contents.data, contents.cachable,
                             contents.compression_type);
  Block reader1(std::move(contents), kDisableGlobalSequenceNumber);
  Block reader2(std::move(contents_ref), kDisableGlobalSequenceNumber);

  std::unique_ptr<const SliceTransform> prefix_extractor(
      NewFixedPrefixTransform(prefix_size));

  std::unique_ptr<InternalIterator> regular_iter(
      reader2.NewIterator(BytewiseComparator(), BytewiseComparator()));

  // Seek existent keys
  for (size_t i = 0; i < keys.size(); i++) {
    regular_iter->Seek(keys[i]);
    ASSERT_OK(regular_iter->status());
    ASSERT_TRUE(regular_iter->Valid());

    Slice v = regular_iter->value();
    ASSERT_EQ(v.ToString().compare(values[i]), 0);
  }

  // Seek non-existent keys.
  // For hash index, if no key with a given prefix is not found, iterator will
  // simply be set as invalid; whereas the binary search based iterator will
  // return the one that is closest.
  for (int i = 1; i < max_key - 1; i += 2) {
    auto key = GenerateKey(i, 0, 0, nullptr);
    regular_iter->Seek(key);
    ASSERT_TRUE(regular_iter->Valid());
  }
}

void SuffixIndexPerf(bool use_suffix_index = false, size_t key_size = 10) {
  Random rnd(301);
  Options options = Options();
  std::unique_ptr<InternalKeyComparator> ic;
  ic.reset(new test::PlainInternalKeyComparator(options.comparator));

  std::vector<std::string> keys;
  std::vector<std::string> values;


  BlockBuilder builder(1 /* block_restart_interval */,
                       true /* use_delta_encoding */,
                       use_suffix_index);
  int num_records = 500;

  GenerateRandomKVs(&keys, &values, 0, num_records);
//  keys.push_back("key1");
//  values.push_back("value1");
  // add a bunch of records to a block
  for (int i = 0; i < num_records; i++) {
    keys[i].resize(key_size);
    builder.Add(keys[i], values[i]);
  }

  // read serialized contents of the block
  Slice rawblock = builder.Finish();

  // create block reader
  BlockContents contents;
  contents.data = rawblock;
  contents.cachable = false;
  Block reader(std::move(contents), kDisableGlobalSequenceNumber,
               0 /* read_amp_bytes_per_bit */,
               nullptr /* statistics */,
               use_suffix_index);

  // read contents of block sequentially
  int count = 0;
  InternalIterator *iter =
      reader.NewIterator(options.comparator, options.comparator);
  for (iter->SeekToFirst();iter->Valid(); count++, iter->Next()) {

    // read kv from block
    Slice k = iter->key();
    Slice v = iter->value();

    // compare with lookaside array
    ASSERT_EQ(k.ToString().compare(keys[count]), 0);
    ASSERT_EQ(v.ToString().compare(values[count]), 0);
  }
  delete iter;

  // read block contents randomly
  iter = reader.NewIterator(options.comparator, options.comparator);
  for (int j = 0; j < 100000; j++) {
//    std::cout << j << "\n";
    for (int i = 0; i < num_records; i++) {

      // find a random key in the lookaside array
      int index = rnd.Uniform(num_records);
      Slice k(keys[index]);

      // search in block for this key
      iter->Seek(k);
      ASSERT_TRUE(iter->Valid());
      Slice v = iter->value();
      ASSERT_EQ(v.ToString().compare(values[index]), 0);
    }
  }
  delete iter;
}

}  // namespace rocksdb

int main(int argc, char **argv) {
  bool use_suffix_index = false;
  size_t key_size = 10;
  if (argc >= 2) {
    use_suffix_index = strcmp(argv[1], "0");
  }

  if (argc >= 3) {
    key_size = std::stoi(argv[2]);
  }

  rocksdb::SuffixIndexPerf(use_suffix_index, key_size);
  return 0;
}
