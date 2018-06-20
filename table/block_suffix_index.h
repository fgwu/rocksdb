#ifndef BLOCK_SUFFIX_INDEX_H
#define BLOCK_SUFFIX_INDEX_H

namespace rocksdb {

class BlockSuffixIndex {
 public:

  bool Seek(const Slice& target, uint32_t* index);

  size_t ApproximateMemoryUsage() const {
    return 0; // TODO(fwu)
    // return sizeof(BlockSuffixIndex) +
    //   (num_block_array_buffer_entries_ + num_buckets_) * sizeof(uint32_t);
  }

 private:
};

}  // namespace rocksdb
#endif
