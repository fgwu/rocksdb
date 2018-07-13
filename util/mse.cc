//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <cmath>
#include <iostream>
#include "rocksdb/slice.h"
#include "util/mse.h"
#include "util/coding.h"

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

void Mse::Reset() {
  cnt_ = 0;
  t_sum_ = 0;
  y_sum_ = 0;
  t2_sum_ = 0;
  y2_sum_ = 0;
  ty_sum_ = 0;
}

void MseIndex::Add(Slice slice, double rank) {
  if (actual_cnt_ == 0) {
    // this is the first key of the block.
    // TODO(fwu): can we avoid this memcpy?
    first_key_.assign(slice.data(), slice.size());
  }
  actual_cnt_++;

  Slice suffix = ExtractSuffix(slice);
  dout << "base=" << base_ << " prefix_len=" << prefix_len_ << "\n";
  mse_.Add(SliceToDouble(suffix), (double) rank);
}

void MseIndex::Finish() {
  corr_coef_ = mse_.Finish(b0_, b1_);
}

void MseIndex::Finish(std::string& buffer, const Slice& last_key) {
  actual_prefix_len_ = CommonPrefixLen(Slice(first_key_), last_key);

  // if we successfully predict the prefix_len_
  if (actual_prefix_len_ >= prefix_len_) {
    Finish();
  }

  dout << "corr_coef=" << corr_coef_ << "\n";

  assert(sizeof(double) == 8);
  assert(sizeof(size_t) == 8);
  dout << "b0=" << b0_ << " b1=" << b1_
     << " prefix=" << prefix_len_ << " base=" << base_
     << " corr_coef=" << corr_coef_ << "\n";
  PutFixed64(&buffer, *(uint64_t*)(&b0_));
  PutFixed64(&buffer, *(uint64_t*)(&b1_));
  PutFixed64(&buffer, static_cast<uint64_t>(prefix_len_));
  PutFixed64(&buffer, *(uint64_t*)(&base_));
  PutFixed64(&buffer, *(uint64_t*)(&corr_coef_));
}



double MseIndex::Seek(Slice slice) {
  Slice suffix = ExtractSuffix(slice);
  dout << suffix.ToString() << " " << SliceToDouble(suffix) << "\n";
  dout << b0_ << " " << b1_ << " "
       <<  b0_ + b1_ * SliceToDouble(suffix) << "\n";
  return b0_ + b1_ * SliceToDouble(suffix);
}

bool MseIndex::Seek(Slice slice, uint32_t* index) {
  if (corr_coef_ > 5) {
    return false;
  }

  double raw = Seek(slice);
  // avoid undefined behavior when casting a negative double to uint32_t
  if (raw < 0) {
    raw = 0;
  }

  *index = static_cast<unsigned int>(round(raw));
  return true;
}

double MseIndex::SliceToDouble(Slice slice) {
  double t = 0;
  double base = base_;
  for (size_t i = 0; i < slice.size(); i++, base/=256) {
    t += (double)slice[i] * base;
//    dout << "slice to double [" << i << "]:" << slice[i] << " " << t << "\n";
  }

  return t;
}

// reset all the parameters for the next block
// estimate the prefix_len_ and cnt_ for the next block
// reset the actual_prefix_len_ and actual_cnt_ for next block.
void MseIndex::Reset() {
  mse_.Reset();
  b0_ = 0;
  b1_ = 0;
  corr_coef_ = 10;

  cnt_ = actual_cnt_;
  // estimate the length of prefix of the next block.
  // subtracting 2 to make a safe margin.
  // If last prefix_len is less than 2, set it to 0
  if (actual_prefix_len_ < 2) {
    prefix_len_ = 0;
  } else {
    prefix_len_ = actual_prefix_len_ - 2;
  }

  actual_cnt_ = 0;
  actual_prefix_len_ = 0; // doesnt' matter, will be set in Finish();

  long base = 1;
  for (; base < cnt_; base <<= 1){
    ; // do nothing
  }
  base_ = base;
}

}  // namespace rocksdb
