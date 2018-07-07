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


TEST_F(MseTest, MseSliceTest) {
  MseSlice mse_slice(Slice("aaaaaaaa"), Slice("aaaaaaaf"), 3);
  // the estimiated rank cannot be exactly the same.
  // So we consider them equal if their difference is less that `err_limit`
  double err_limit = 0.4;
  mse_slice.Add(Slice("aaaaaaaa"), 0.0);
  mse_slice.Add(Slice("aaaaaaab"), 1.0);
  //                  "aaaaaaac"   2
  //                  "aaaaaaad"   3
  mse_slice.Add(Slice("aaaaaaae"), 4.0);
  mse_slice.Finish();

  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaaa")) - 0.0), err_limit);
  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaab")) - 1.0), err_limit);
  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaac")) - 2.0), err_limit);
  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaad")) - 3.0), err_limit);
  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaae")) - 4.0), err_limit);
  ASSERT_LE(abs(mse_slice.Seek(Slice("aaaaaaaf")) - 5.0), err_limit);
}


}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
