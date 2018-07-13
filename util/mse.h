//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <iostream>
#include "rocksdb/slice.h"
#include "util/coding.h"

#define DBG false

#define dout if (DBG)						\
    std::cout << "["<< __FILE__ << ":" << __FUNCTION__ << "] "

namespace rocksdb {

class Mse {
 public:
  Mse(): cnt_(0), t_sum_(0), y_sum_(0), t2_sum_(0), y2_sum_(0), ty_sum_(0) {}
  void Add(double t, double y);
  double Finish(double& b0, double& b1);
  void Reset();

 private:
  long cnt_;
  double t_sum_;
  double y_sum_;
  double t2_sum_;
  double y2_sum_;
  double ty_sum_;
};

class MseIndex {
 public:
  MseIndex(): b0_(0), b1_(0),
              corr_coef_(10),// corr_coef should [-1, 1]. 10 means invalid
              cnt_(-1), // -1 means first_, last_, and cnt_ is not set.
              prefix_len_(0), base_(1),
              actual_cnt_(0){}

  MseIndex(Slice first, Slice last, long cnt):
      b0_(0), b1_(0),
      corr_coef_(10), // corr_coef should [-1, 1]. 10 means invalid
      first_(first),
      last_(last),
      cnt_(cnt), actual_cnt_(0) {
    prefix_len_ = CommonPrefixLen(first, last);
    long base = 1;
    for (; base < cnt; base <<= 1){
      ; // do nothing
    }
    base_ = base;
  }

  MseIndex(size_t prefix_len, long cnt):
      b0_(0), b1_(0),
      corr_coef_(10), // corr_coef should [-1, 1]. 10 means invalid
      cnt_(cnt),
      prefix_len_(prefix_len),
      actual_cnt_(0){
    long base = 1;
    for (; base < cnt; base <<= 1){
      ; // do nothing
    }
    base_ = base;
//    std::cout << "cnstr: prefix_len_=" << prefix_len_ << " cnt_=" << cnt_ << " ";
  }

  MseIndex(Slice block_content) {
    assert(block_content.size() >=
           4 * sizeof(uint64_t) + sizeof(uint64_t));
    const char* data = block_content.data();
    size_t size = static_cast<uint32_t>(block_content.size());
    uint64_t num_buf = DecodeFixed64(data + size - 5 * sizeof(uint64_t));
    b0_ = *(double*)(&num_buf);
    num_buf = DecodeFixed64(data + size - 4 * sizeof(uint64_t));
    b1_ = *(double*)(&num_buf);
    prefix_len_ = DecodeFixed64(data + size - 3 * sizeof(uint64_t));
    num_buf = DecodeFixed64(data + size - 2 * sizeof(uint64_t));
    base_ = *(double*)(&num_buf);
    num_buf = DecodeFixed64(data + size - sizeof(uint64_t));
    corr_coef_ = *(double*)(&num_buf);
    dout << "b0=" << b0_ << " b1=" << b1_
         << " prefix=" << prefix_len_ << " base=" << base_
         << " corr_coef=" << corr_coef_ << "\n";
  }

  void Add(Slice slice, double rank);
  void Finish();
  void Finish(std::string& buffer, const Slice& last_key);
  double Seek(Slice slice);
  bool Seek(Slice slice, uint32_t* index);
  void Reset();

  static size_t CommonPrefixLen(Slice a, Slice b){
    size_t i = 0;
    for (; i < a.size() && i < b.size(); i++) {
      if (a[i] != b[i])
        break;
    }
    return i;
  }

 private:
  double SliceToDouble(Slice slice);

  Slice ExtractSuffix(Slice slice) {
    assert(prefix_len_ <= slice.size());
    return Slice(slice.data() + prefix_len_, slice.size() - prefix_len_);
  }


  Mse mse_;
  double b0_;
  double b1_;
  double corr_coef_;
  Slice first_;
  Slice last_;
  long cnt_;
  size_t prefix_len_;
  double base_;

  long actual_cnt_;
  size_t actual_prefix_len_;
  std::string first_key_;
};


} // namespace rocksdb
