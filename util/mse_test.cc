//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <string>

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

}  // namespace rocksdb

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
