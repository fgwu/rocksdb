// Copyright (c) 2011-present, Facebook, Inc. All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <cstdlib>
#include <string>
#include <unordered_map>

#include "rocksdb/slice.h"
#include "table/block.h"
#include "table/block_builder.h"
#include "table/data_block_hash_index.h"
#include "util/testharness.h"
#include "util/testutil.h"

namespace rocksdb {

bool SearchForOffset(DataBlockHashIndex& index, const char* data,
                     uint16_t map_offset, const Slice& key,
                     uint8_t& restart_point) {
  uint8_t entry = index.Seek(data, map_offset, key);
  if (entry == kCollision) {
    return true;
  }

  if (entry == kNoEntry) {
    return false;
  }

  return entry == restart_point;
}

// Random KV generator similer to block_test
static std::string RandomString(Random* rnd, int len) {
  std::string r;
  test::RandomString(rnd, len, &r);
  return r;
}
std::string GenerateKey(int primary_key, int secondary_key, int padding_size,
                        Random *rnd) {
  char buf[50];
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

TEST(DataBlockHashIndex, DataBlockHashTestSmall) {
  DataBlockHashIndexBuilder builder(0.75);
  for (int j = 0; j < 5; j++) {
    for (uint8_t i = 0; i < 2 + j; i++) {
      std::string key("key" + std::to_string(i));
      uint8_t restart_point = i;
      builder.Add(key, restart_point);
    }

    size_t estimated_size = builder.EstimateSize();

    std::string buffer("fake"), buffer2;
    size_t original_size = buffer.size();
    estimated_size += original_size;
    builder.Finish(buffer);

    ASSERT_EQ(buffer.size(), estimated_size);

    buffer2 = buffer; // test for the correctness of relative offset


    Slice s(buffer2);
    DataBlockHashIndex index;
    uint16_t map_offset;
    index.Initialize(s.data(), s.size(), &map_offset);

    // the additional hash map should start at the end of the buffer
    ASSERT_EQ(original_size, map_offset);
    for (uint8_t i = 0; i < 2; i++) {
      std::string key("key" + std::to_string(i));
      uint8_t restart_point = i;
      ASSERT_TRUE(SearchForOffset(index, s.data(), map_offset, key, restart_point));
    }
    builder.Reset(2 + j);
    ASSERT_EQ(builder.num_buckets_,
              static_cast<uint16_t>((static_cast<double>(2 + j) / 0.75)));
  }
}

TEST(DataBlockHashIndex, DataBlockHashTest) {
  // bucket_num = 200, #keys = 100. 50% utilization
  DataBlockHashIndexBuilder builder(0.75);

  for (uint8_t i = 0; i < 100; i++) {
    std::string key("key" + std::to_string(i));
    uint8_t restart_point = i;
    builder.Add(key, restart_point);
  }

  size_t estimated_size = builder.EstimateSize();

  std::string buffer("fake content"), buffer2;
  size_t original_size = buffer.size();
  estimated_size += original_size;
  builder.Finish(buffer);

  ASSERT_EQ(buffer.size(), estimated_size);

  buffer2 = buffer; // test for the correctness of relative offset

  Slice s(buffer2);
  DataBlockHashIndex index;
  uint16_t map_offset;
  index.Initialize(s.data(), s.size(), &map_offset);

  // the additional hash map should start at the end of the buffer
  ASSERT_EQ(original_size, map_offset);
  for (uint8_t i = 0; i < 100; i++) {
    std::string key("key" + std::to_string(i));
    uint8_t restart_point = i;
    ASSERT_TRUE(SearchForOffset(index, s.data(), map_offset, key, restart_point));
  }
}

TEST(DataBlockHashIndex, DataBlockHashTestCollision) {
  // bucket_num = 2. There will be intense hash collisions
  DataBlockHashIndexBuilder builder(0.75);

  for (uint8_t i = 0; i < 100; i++) {
    std::string key("key" + std::to_string(i));
    uint8_t restart_point = i;
    builder.Add(key, restart_point);
  }

  size_t estimated_size = builder.EstimateSize();

  std::string buffer("some other fake content to take up space"), buffer2;
  size_t original_size = buffer.size();
  estimated_size += original_size;
  builder.Finish(buffer);

  ASSERT_EQ(buffer.size(), estimated_size);

  buffer2 = buffer; // test for the correctness of relative offset

  Slice s(buffer2);
  DataBlockHashIndex index;
  uint16_t map_offset;
  index.Initialize(s.data(), s.size(), &map_offset);

  // the additional hash map should start at the end of the buffer
  ASSERT_EQ(original_size, map_offset);
  for (uint8_t i = 0; i < 100; i++) {
    std::string key("key" + std::to_string(i));
    uint8_t restart_point = i;
    ASSERT_TRUE(SearchForOffset(index, s.data(), map_offset, key, restart_point));
  }
}

TEST(DataBlockHashIndex, DataBlockHashTestLarge) {
  DataBlockHashIndexBuilder builder(0.75);
  std::unordered_map<std::string, uint8_t> m;

  for (uint8_t i = 0; i < 100; i++) {
    if (i % 2) {
      continue;  // leave half of the keys out
    }
    std::string key = "key" + std::to_string(i);
    uint8_t restart_point = i;
    builder.Add(key, restart_point);
    m[key] = restart_point;
  }

  size_t estimated_size = builder.EstimateSize();

  std::string buffer("filling stuff"), buffer2;
  size_t original_size = buffer.size();
  estimated_size += original_size;
  builder.Finish(buffer);

  ASSERT_EQ(buffer.size(), estimated_size);

  buffer2 = buffer; // test for the correctness of relative offset

  Slice s(buffer2);
  DataBlockHashIndex index;
  uint16_t map_offset;
  index.Initialize(s.data(), s.size(), &map_offset);

  // the additional hash map should start at the end of the buffer
  ASSERT_EQ(original_size, map_offset);
  for (uint8_t i = 0; i < 100; i++) {
    std::string key = "key" + std::to_string(i);
    uint8_t restart_point = i;
    if (m.count(key)) {
      ASSERT_TRUE(m[key] == restart_point);
      ASSERT_TRUE(SearchForOffset(index, s.data(), map_offset, key, restart_point));
    } else {
      // we allow false positve, so don't test the nonexisting keys.
      // when false positive happens, the search will continue to the
      // restart intervals to see if the key really exist.
    }
  }
}

TEST(DataBlockHashIndex, BlockTestSingleKey) {
  Options options = Options();

  BlockBuilder builder(16 /* block_restart_interval */,
                       true /* use_delta_encoding */,
                       BlockBasedTableOptions::kDataBlockHashSearch);

  std::string ukey("gopher");
  std::string value("gold");
  InternalKey ikey(ukey, 10, kTypeValue);
  builder.Add(ikey.Encode().ToString(), value /*value*/);

  // read serialized contents of the block
  Slice rawblock = builder.Finish();

  // create block reader
  BlockContents contents;
  contents.data = rawblock;
  contents.cachable = false;
  Block reader(std::move(contents), kDisableGlobalSequenceNumber);

  auto iter = reader.NewIterator<DataBlockIter>(options.comparator,
                                                options.comparator);

  bool hash_effective, found;
  // search in block for the key just inserted
  {
    InternalKey seek_ikey(ukey, 10, kValueTypeForSeek);
    iter->SeekForGet(ikey.Encode().ToString(), &hash_effective, &found);
    ASSERT_TRUE(hash_effective);
    ASSERT_TRUE(found);
    ASSERT_TRUE(iter->Valid());
    ASSERT_EQ(options.comparator->Compare(
                  iter->key(), ikey.Encode().ToString()), 0);
    ASSERT_EQ(iter->value(), value);
  }

  // search in block for a non-existent ukey
  {
    std::string non_exist_ukey("squirrel");
    InternalKey seek_ikey(non_exist_ukey, 10, kValueTypeForSeek);
    iter->SeekForGet(seek_ikey.Encode().ToString(), &hash_effective, &found);
    ASSERT_TRUE(hash_effective);
    ASSERT_FALSE(found);
    // if not found iter->Valid() is undefined
  }

  // search in block for the existing ukey, but with higher seqno
  {
    InternalKey seek_ikey(ukey, 20, kValueTypeForSeek);

    // searching "gopher@20"
    iter->SeekForGet(ikey.Encode().ToString(), &hash_effective, &found);
    ASSERT_TRUE(hash_effective);
    ASSERT_TRUE(found);
    ASSERT_TRUE(iter->Valid());

    // user key should match
    ASSERT_EQ(options.comparator->Compare(
                  ExtractUserKey(iter->key()), ukey), 0);

    // seek_key seqno number should be greater than that of iter result
    ASSERT_GT(GetInternalKeySeqno(seek_ikey.Encode()),
              GetInternalKeySeqno(iter->key()));

    ASSERT_EQ(iter->value(), value);
  }

  // Search in block for the existing ukey, but with lower seqno
  // in this case, seek can find the first occurrence of the user_key, but
  // ParseNextDataKey() will skip it as it does not have a older seqno.
  // In this case, GetForSeek() is effective to locate the user_key, and
  // iter->Valid() == false indicates that we've reached to the end of
  // the block and the caller should continue searching the next block.
  {
    InternalKey seek_ikey(ukey, 5, kValueTypeForSeek);
    iter->SeekForGet(seek_ikey.Encode().ToString(), &hash_effective, &found);
    ASSERT_TRUE(hash_effective);
    ASSERT_FALSE(iter->Valid());
  }

  delete iter;
}

TEST(DataBlockHashIndex, BlockTestLarge) {
  Random rnd(1019);
  Options options = Options();
  std::unique_ptr<InternalKeyComparator> ic;
  ic.reset(new test::PlainInternalKeyComparator(options.comparator));

  std::vector<std::string> keys;
  std::vector<std::string> values;

  BlockBuilder builder(16 /* block_restart_interval */,
                       true /* use_delta_encoding */,
                       BlockBasedTableOptions::kDataBlockHashSearch);
  int num_records = 500;

  GenerateRandomKVs(&keys, &values, 0, num_records);

  // Generate keys. Adding a trailing "1" to indicate existent keys.
  // Later will Seeking for keys with a trailing "0" to test seeking
  // non-existent keys.
  for (int i = 0; i < num_records; i++) {
    std::string ukey(keys[i] + "1" /* existing key marker */);
    InternalKey ikey(ukey, 0, kTypeValue);
    builder.Add(ikey.Encode().ToString(), values[i]);
  }

  // read serialized contents of the block
  Slice rawblock = builder.Finish();

  // create block reader
  BlockContents contents;
  contents.data = rawblock;
  contents.cachable = false;
  Block reader(std::move(contents), kDisableGlobalSequenceNumber);

  // random seek existent keys
  for (int i = 0; i < num_records; i++) {
    auto iter = reader.NewIterator<DataBlockIter>(options.comparator,
                                                  options.comparator);
    // find a random key in the lookaside array
    int index = rnd.Uniform(num_records);
    std::string ukey(keys[index] + "1" /* existing key marker */);
    InternalKey ikey(ukey, 0, kTypeValue);

    // search in block for this key
    bool hash_effective, found;
    iter->SeekForGet(ikey.Encode().ToString(), &hash_effective, &found);
    if (hash_effective) {
      ASSERT_TRUE(found);
      ASSERT_TRUE(iter->Valid());
      Slice v = iter->value();
      ASSERT_EQ(v.ToString().compare(values[index]), 0);
    }
    delete iter;
  }

  // random seek non-existent keys
  for (int i = 0; i < num_records; i++) {
    auto iter = reader.NewIterator<DataBlockIter>(options.comparator,
                                                  options.comparator);
    // find a random key in the lookaside array
    int index = rnd.Uniform(num_records);
    std::string ukey(keys[index] + "0" /* existing key marker */);
    InternalKey ikey(ukey, 0, kTypeValue);

    // search in block for this key
    bool hash_effective, found;
    iter->SeekForGet(ikey.Encode().ToString(), &hash_effective, &found);
    if (hash_effective) {
      ASSERT_FALSE(found);
    }
    delete iter;
  }
}

}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
