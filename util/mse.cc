//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <cmath>
#include <iostream>
#include "rocksdb/slice.h"
#include "util/mse.h"

namespace rocksdb {

void Mse::Add(double t, double y) {
  cnt_++;
  t_sum_ += t;
  y_sum_ += y;
  t2_sum_ += t * t;
  y2_sum_ += y * y;
  ty_sum_ += t * y;
}

double Mse::Finish(double& b0, double& b1) {
  double e_t = t_sum_ / cnt_;
  double e_y = y_sum_ / cnt_;
  double e_t2 = t2_sum_ / cnt_;
  double e_y2 = y2_sum_ / cnt_;
  double e_ty = ty_sum_ / cnt_;

  double cov_ty = e_ty - e_t * e_y;
  double var_t = e_t2 - e_t * e_t;
  double var_y = e_y2 - e_y * e_y;

  b1 = cov_ty / var_t;
  b0 = e_y - b1 * e_t;

  dout << b1 << " " << b0 << " " << "\n";

  // TODO(fwu), sqrt really needed here?
  double corr_coef = cov_ty / sqrt(var_t * var_y);

  return corr_coef;
}

void MseSlice::Add(Slice slice, double rank) {
  Slice suffix = ExtractSuffix(slice);
  dout << base_ << " " << prefix_len_ << "\n";
  mse_.Add(SliceToDouble(suffix), (double) rank);
}

void MseSlice::Finish() {
  corr_coef_ = mse_.Finish(b0_, b1_);
}

double MseSlice::Seek(Slice slice) {
  assert(corr_coef_ <= 2); // index should be valid
  Slice suffix = ExtractSuffix(slice);

  dout << suffix.ToString() << " " << SliceToDouble(suffix) << "\n";
  return b0_ + b1_ * SliceToDouble(suffix);
}

double MseSlice::SliceToDouble(Slice slice) {
  double t = 0;
  double base = base_;
  for (size_t i = 0; i < slice.size(); i++, base/=256) {
    dout << "slice to double " << slice[i] << " " << t << "\n";
    t += (double)slice[i] * base;
  }
  return t;
}

}  // namespace rocksdb
