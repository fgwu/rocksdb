//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <string>
#include <cmath>

#include "util/mse.h"
#include "util/testharness.h"

namespace rocksdb {

class MseTest : public testing::Test {};

TEST_F(MseTest, SimpleTest) {
  Mse mse;
  mse.Add(1,3);
  mse.Add(2,5);
  mse.Add(3,7);

  double b0, b1, corr_coef;
  corr_coef = mse.Finish(b0, b1);

  ASSERT_EQ(b0, 1);
  ASSERT_EQ(b1, 2);
  ASSERT_EQ(corr_coef, 1);
}

TEST_F(MseTest, SimpleTest2) {
  Mse mse;
  mse.Add(3,3);
  mse.Add(5,2);
  mse.Add(7,1);

  double b0, b1, corr_coef;
  corr_coef = mse.Finish(b0, b1);

  ASSERT_EQ(b0, 4.5);
  ASSERT_EQ(b1, -0.5);
  ASSERT_EQ(corr_coef, -1);
}


TEST_F(MseTest, MseIndexTest) {
  MseIndex mse_index(1, 3);
  // the estimiated rank cannot be exactly the same.
  // So we consider them equal if their difference is less that `err_limit`
  double err_limit = 0.001;
  mse_index.Add(Slice("aa"), 0.0);
  mse_index.Add(Slice("ab"), 1.0);
  //                  "ac"   2
  //                  "ad"   3
  mse_index.Add(Slice("ae"), 4.0);
  mse_index.Finish();

  ASSERT_LE(abs(mse_index.Seek(Slice("aa")) - 0.0), err_limit);
  ASSERT_LE(abs(mse_index.Seek(Slice("ab")) - 1.0), err_limit);
  ASSERT_LE(abs(mse_index.Seek(Slice("ac")) - 2.0), err_limit);
  ASSERT_LE(abs(mse_index.Seek(Slice("ad")) - 3.0), err_limit);
  ASSERT_LE(abs(mse_index.Seek(Slice("ae")) - 4.0), err_limit);
  ASSERT_LE(abs(mse_index.Seek(Slice("af")) - 5.0), err_limit);
}

TEST_F(MseTest, MseIndexPrefixTest) {
    double err_limit = 0.001;
    for (int i = 0; i < 2; i++) {
      std::unique_ptr<MseIndex> mse_index;
      if (i == 0) {
        mse_index.reset(
            new MseIndex(Slice("aaaaaaaa"), Slice("aaaaaaaf"), 3));
      } else {
        mse_index.reset(
            new MseIndex(7, 3));
      }
      // the estimiated rank cannot be exactly the same.
      // So we consider them equal if their difference is less that `err_limit`
      mse_index->Add(Slice("aaaaaaaa"), 0.0);
      mse_index->Add(Slice("aaaaaaab"), 1.0);
      //                  "aaaaaaac"   2
      //                  "aaaaaaad"   3
      mse_index->Add(Slice("aaaaaaae"), 4.0);
      mse_index->Finish();

      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaaa")) - 0.0), err_limit);
      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaab")) - 1.0), err_limit);
      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaac")) - 2.0), err_limit);
      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaad")) - 3.0), err_limit);
      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaae")) - 4.0), err_limit);
      ASSERT_LE(abs(mse_index->Seek(Slice("aaaaaaaf")) - 5.0), err_limit);
    }
}

TEST_F(MseTest, MseIndexBuildAndRead) {
    double err_limit = 1;
    std::string block_content = "block content";
    MseIndex mse_index_builder(6, 5);

    mse_index_builder.Add(Slice("aaaaaaaa"), 0.0);
    mse_index_builder.Add(Slice("aaaaaaab"), 1.0);
    mse_index_builder.Add(Slice("aaaaaaac"), 2.0);
    mse_index_builder.Add(Slice("aaaaaaad"), 3.0);
    mse_index_builder.Add(Slice("aaaaaaae"), 4.0);

    mse_index_builder.Finish(block_content, Slice("aaaaaaae"));

    MseIndex mse_index_reader(block_content);
    uint32_t index;
    ASSERT_TRUE(mse_index_reader.Seek(Slice("aaaaaaaa"), &index));
    ASSERT_LE(abs(index - 0.0), err_limit);
    ASSERT_TRUE(mse_index_reader.Seek(Slice("aaaaaaab"), &index));
    ASSERT_LE(abs(index - 1.0), err_limit);
    ASSERT_TRUE(mse_index_reader.Seek(Slice("aaaaaaac"), &index));
    ASSERT_LE(abs(index - 2.0), err_limit);
    ASSERT_TRUE(mse_index_reader.Seek(Slice("aaaaaaad"), &index));
    ASSERT_LE(abs(index - 3.0), err_limit);
    ASSERT_TRUE(mse_index_reader.Seek(Slice("aaaaaaae"), &index));
    ASSERT_LE(abs(index - 4.0), err_limit);

    // here actual_prefix_len = 7, so the next estimated prefix_len = 5
    mse_index_builder.Reset();


    // we delibrately add a set of strings with prefix_len = 4.
    mse_index_builder.Add(Slice("xxxxa"), 10.0);
    mse_index_builder.Add(Slice("xxxxb"), 11.0);
    mse_index_builder.Add(Slice("xxxxc"), 12.0);
    mse_index_builder.Add(Slice("xxxxd"), 13.0);
    mse_index_builder.Add(Slice("xxxxe"), 14.0);

    std::string block_content2 = "block content2";
    mse_index_builder.Finish(block_content2, Slice("xxxxe"));

    MseIndex mse_index_reader2(block_content2);
    // So the builder fails. Subsequent seeks should fail
    ASSERT_FALSE(mse_index_reader2.Seek(Slice("xxxxa"), &index));
    ASSERT_FALSE(mse_index_reader2.Seek(Slice("xxxxb"), &index));
    ASSERT_FALSE(mse_index_reader2.Seek(Slice("xxxxc"), &index));
    ASSERT_FALSE(mse_index_reader2.Seek(Slice("xxxxd"), &index));
    ASSERT_FALSE(mse_index_reader2.Seek(Slice("xxxxe"), &index));
}


}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
