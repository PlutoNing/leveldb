// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_VERSION_EDIT_H_
#define STORAGE_LEVELDB_DB_VERSION_EDIT_H_

#include "db/dbformat.h"
#include <set>
#include <utility>
#include <vector>

namespace leveldb {

class VersionSet;

struct FileMetaData {
  FileMetaData() : refs(0), allowed_seeks(1 << 30), file_size(0) {}

  int refs;  // 还能被seek的次数，低于0就要被compact

  int allowed_seeks; /**
  当某SSTable的allowed_seeks减为0时，会触发seek
  compaction，该SSTable会与下层部分SSTable合并。*/
  uint64_t number; //文件id
  uint64_t file_size;    // File size in bytes
  InternalKey smallest;  // 最小key
  InternalKey largest;   // 最大key
};
/**
 * 1 当版本间有增量变动时，VersionEdit记录了这种变动； 2
 * 写入到MANIFEST时，先将current
 * version的db元信息保存到一个VersionEdit中，然后在组织成一个log
 * record写入文件；
 */
class VersionEdit {
 public:
  VersionEdit() { Clear(); }
  ~VersionEdit() = default;

  void Clear();  // 清空信息

  void SetComparatorName(const Slice& name) {
    has_comparator_ = true;
    comparator_ = name.ToString();
  }
  void SetLogNumber(uint64_t num) {
    has_log_number_ = true;
    log_number_ = num;
  }
  void SetPrevLogNumber(uint64_t num) {
    has_prev_log_number_ = true;
    prev_log_number_ = num;
  }
  void SetNextFile(uint64_t num) {
    has_next_file_number_ = true;
    next_file_number_ = num;
  }
  void SetLastSequence(SequenceNumber seq) {
    has_last_sequence_ = true;
    last_sequence_ = seq;
  }
  void SetCompactPointer(int level, const InternalKey& key) {
    compact_pointers_.push_back(std::make_pair(level, key));
  }

  // Add the specified file at the specified number.
  // REQUIRES: This version has not been saved (see VersionSet::SaveTo)
  // REQUIRES: "smallest" and "largest" are smallest and largest keys in file
  /**
   * // 添加sstable文件信息，要求：DB元信息还没有写入磁盘Manifest文件
// @level：.sst文件层次；@file 文件编号-用作文件名 @size 文件大小
// @smallest, @largest：sst文件包含k/v对的最大最小key
  */
  void AddFile(int level, uint64_t file, uint64_t file_size,
               const InternalKey& smallest, const InternalKey& largest) {
    FileMetaData f;
    f.number = file;
    f.file_size = file_size;
    f.smallest = smallest;
    f.largest = largest;
    new_files_.push_back(std::make_pair(level, f));
  }

  // 从指定的level删除文件
  void RemoveFile(int level, uint64_t file) {
    deleted_files_.insert(std::make_pair(level, file));
  }
  // 将信息Encode到一个string中
  void EncodeTo(std::string* dst) const;
  // 从Slice中Decode出DB元信息
  Status DecodeFrom(const Slice& src);

  std::string DebugString() const;

 private:
  friend class VersionSet;

  typedef std::set<std::pair<int, uint64_t>> DeletedFileSet;

  //===================下面是成员变量，由此可大概窥得DB元信息的内容。
  // key comparator名字
  std::string comparator_;
  // 日志编号
  uint64_t log_number_;
  // 前一个日志编号
  uint64_t prev_log_number_;
  // 下一个文件编号
  uint64_t next_file_number_;
  // 上一个seq
  SequenceNumber last_sequence_;
  // 是否有comparator
  bool has_comparator_;
  // 是否有log_number_
  bool has_log_number_;
  // 是否有prev_log_number_
  bool has_prev_log_number_;
  // 是否有next_file_number_
  bool has_next_file_number_;
  // 是否有last_sequence_
  bool has_last_sequence_;
  // compact点
  std::vector<std::pair<int, InternalKey>> compact_pointers_;
  // 删除文件集合
  DeletedFileSet deleted_files_;
  // 新文件集合
  std::vector<std::pair<int, FileMetaData>> new_files_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_VERSION_EDIT_H_
