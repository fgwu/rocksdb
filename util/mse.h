//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

namespace rocksdb {

class Mse {
 public:
  Mse(): cnt_(0), t_sum_(0), y_sum_(0), t2_sum_(0), y2_sum_(0), ty_sum_(0) {}
  void Add(double t, double y);
  double Finish(double& b0, double& b1);

 private:
  long cnt_;
  double t_sum_;
  double y_sum_;
  double t2_sum_;
  double y2_sum_;
  double ty_sum_;
};

} // namespace rocksdb
