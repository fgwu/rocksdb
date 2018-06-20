#include "rocksdb/slice.h"
#include "table/block_suffix_index.h"

namespace rocksdb {

bool BlockSuffixIndex::Seek(const Slice& key,  uint32_t* index) {
// TODO(fwu)
  if (key == key) index = index;
  return false;
}

}  // namespace rocksdb
