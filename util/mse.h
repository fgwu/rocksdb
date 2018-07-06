//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <iostream>
#include "rocksdb/slice.h"

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

class MseSlice {
 public:
  MseSlice(): b0_(0), b1_(0),
              corr_coef_(10){} // corr_coef should [-1, 1]. 10 means invalid
  void Add(Slice slice, double rank);
  void Finish();
  double Seek(Slice slice);

 private:
  inline double SliceToDouble(Slice slice) {
    double t = 0;
    double base = 1;
    for (size_t i = 0; i < slice.size(); i++, base/=256) {
      t += (double)slice[i] * base;
    }

    return t;
  }

  Mse mse_;
  double b0_;
  double b1_;
  double corr_coef_;
};


} // namespace rocksdb
