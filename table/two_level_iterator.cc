// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/two_level_iterator.h"

#include "leveldb/table.h"

#include "table/block.h"
#include "table/format.h"
#include "table/iterator_wrapper.h"

namespace leveldb {

namespace {

typedef Iterator* (*BlockFunction)(void*, const ReadOptions&, const Slice&);
/**
 * 之所以叫two
 * level应该是不仅可以迭代其中存储的对象，它还接受了一个函数BlockFunction，
 * 可以遍历存储的对象，可见它是专门为Table定制的。
 */
class TwoLevelIterator : public Iterator {
 public:
  TwoLevelIterator(Iterator* index_iter, BlockFunction block_function,
                   void* arg, const ReadOptions& options);

  ~TwoLevelIterator() override;

  void Seek(const Slice& target) override;
  void SeekToFirst() override;
  void SeekToLast() override;
  void Next() override;
  void Prev() override;

  bool Valid() const override { return data_iter_.Valid(); }
  // 其Key和Value接口都是返回的data_iter_对应的key和value：
  Slice key() const override {
    assert(Valid());
    return data_iter_.key();
  }
  // 其Key和Value接口都是返回的data_iter_对应的key和value：
  Slice value() const override {
    assert(Valid());
    return data_iter_.value();
  }
  Status status() const override {
    // It'd be nice if status() returned a const Status& instead of a Status
    if (!index_iter_.status().ok()) {
      return index_iter_.status();
    } else if (data_iter_.iter() != nullptr && !data_iter_.status().ok()) {
      return data_iter_.status();
    } else {
      return status_;
    }
  }

 private:
  void SaveError(const Status& s) {
    if (status_.ok() && !s.ok()) status_ = s;
  }
  void SkipEmptyDataBlocksForward();
  void SkipEmptyDataBlocksBackward();
  void SetDataIterator(Iterator* data_iter);
  void InitDataBlock();
  /**
   * 我们已经知道各种Block的存储格式都是相同的，但是各自block
   * data存储的k/v又互不相同，于是我们就需要一个途径，
   * 能够在使用同一个方式遍历不同的block时，又能解析这些k/v。
   * 这就是BlockFunction，它又返回了一个针对block
   * data的Iterator。
   */
  BlockFunction block_function_;
  void* arg_;  
  const ReadOptions options_;
  //上面俩是block func的参数和opt
  Status status_;
  IteratorWrapper index_iter_;  // 遍历block的迭代器
  IteratorWrapper data_iter_;   // 历block data的迭代器  ，May be nullptr
  // If data_iter_ is non-null, then "data_block_handle_" holds the
  // "index_value" passed to block_function_ to create the data_iter_.
  std::string data_block_handle_; //当前遍历的data block handle
};
/**
 * 以参数1的itr构造二级itr
*/
TwoLevelIterator::TwoLevelIterator(Iterator* index_iter,
                                   BlockFunction block_function, void* arg,
                                   const ReadOptions& options)
    : block_function_(block_function),
      arg_(arg),
      options_(options),
      index_iter_(index_iter),
      data_iter_(nullptr) {}

TwoLevelIterator::~TwoLevelIterator() = default;
/**
 * 
*/
void TwoLevelIterator::Seek(const Slice& target) {
  index_iter_.Seek(target);
  //先找到target的data block
  InitDataBlock();
  if (data_iter_.iter() != nullptr) 
  data_iter_.Seek(target);
  //再找这个data block里的target
  SkipEmptyDataBlocksForward();
}
/**
 * 从第一个data block开始
*/
void TwoLevelIterator::SeekToFirst() {
  index_iter_.SeekToFirst();
  InitDataBlock();
  if (data_iter_.iter() != nullptr) data_iter_.SeekToFirst();
  SkipEmptyDataBlocksForward();
}
/**
 * 从最后一个data block开始
*/
void TwoLevelIterator::SeekToLast() {
  index_iter_.SeekToLast();
  InitDataBlock();
  if (data_iter_.iter() != nullptr) data_iter_.SeekToLast();
  SkipEmptyDataBlocksBackward();
}

void TwoLevelIterator::Next() {
  assert(Valid());
  data_iter_.Next();
  SkipEmptyDataBlocksForward();
}

void TwoLevelIterator::Prev() {
  assert(Valid());
  data_iter_.Prev();
  SkipEmptyDataBlocksBackward();
}
/**
 * 顾名思义
*/
void TwoLevelIterator::SkipEmptyDataBlocksForward() {
  while (data_iter_.iter() == nullptr || !data_iter_.Valid()) {
    // Move to next block
    if (!index_iter_.Valid()) {
      SetDataIterator(nullptr);
      return;
    }
    index_iter_.Next();
    //通过index iter找到下一个data block
    InitDataBlock();
    if (data_iter_.iter() != nullptr) data_iter_.SeekToFirst();
  }
}
/**
 * 
*/
void TwoLevelIterator::SkipEmptyDataBlocksBackward() {
  while (data_iter_.iter() == nullptr || !data_iter_.Valid()) {
    // Move to next block
    if (!index_iter_.Valid()) {
      SetDataIterator(nullptr);
      return;
    }
    index_iter_.Prev();
    InitDataBlock();
    if (data_iter_.iter() != nullptr) data_iter_.SeekToLast();
  }
}

void TwoLevelIterator::SetDataIterator(Iterator* data_iter) {
  if (data_iter_.iter() != nullptr) SaveError(data_iter_.status());
  data_iter_.Set(data_iter);
}
/**
 * InitDataBlock()，它是根据index_iter来初始化data_iter，
 * 当定位到新的block时，需要更新data
 * Iterator，指向该block中k/v对的合适位置，函数如下：
 */
void TwoLevelIterator::InitDataBlock() {
  if (!index_iter_.Valid()) {
    SetDataIterator(nullptr);
  } else {
    Slice handle = index_iter_.value();
    if (data_iter_.iter() != nullptr &&
        handle.compare(data_block_handle_) == 0) {
      // data_iter_ is already constructed with this iterator, so
      // no need to change anything
    } else {
      Iterator* iter = (*block_function_)(arg_, options_, handle);
      //调用block func解析当前block，获得里面的kv的iter
      data_block_handle_.assign(handle.data(), handle.size());
      //更新当前的data block
      SetDataIterator(iter);
      //更新iter
    }
  }
}

}  // namespace
/**
 * 以参数一的itr构造二级itr
*/
Iterator* NewTwoLevelIterator(Iterator* index_iter,
                              BlockFunction block_function, void* arg,
                              const ReadOptions& options) {
  return new TwoLevelIterator(index_iter, block_function, arg, options);
}

}  // namespace leveldb
