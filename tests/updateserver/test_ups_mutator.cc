/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * test_ups_mutator.cc for ...
 *
 * Authors:
 *   rizhao <rizhao.ych@taobao.com>
 *
 */
#include <iostream>
#include <sstream>
#include <algorithm>
#include <tblog.h>
#include <gtest/gtest.h>

#include "updateserver/ob_ups_mutator.h"

using namespace std;
using namespace sb::common;
using namespace sb::updateserver;

int init_mem_pool() {
  ob_init_memory_pool(2 * 1024L * 1024L);
  return 0;
}
static int init_mem_pool_ret = init_mem_pool();

void check_string(const ObString& expected, const ObString& real) {
  EXPECT_EQ(expected.length(), real.length());
  if (NULL != expected.ptr() && NULL != real.ptr()) {
    EXPECT_EQ(0, memcmp(expected.ptr(), real.ptr(), expected.length()));
  } else {
    EXPECT_EQ((const char*) NULL, expected.ptr());
    EXPECT_EQ((const char*) NULL, real.ptr());
  }
}

void check_obj(const ObObj& expected, const ObObj& real) {
  // TODO
}

void check_cell(const ObCellInfo& expected, const ObCellInfo& real) {
  EXPECT_EQ(expected.column_id_, real.column_id_);
  EXPECT_EQ(expected.table_id_, real.table_id_);
  check_string(expected.row_key_, real.row_key_);
}

void check_cell_with_name(const ObCellInfo& expected, const ObCellInfo& real) {
  check_string(expected.table_name_, real.table_name_);
  check_string(expected.row_key_, real.row_key_);
  check_string(expected.column_name_, real.column_name_);
  check_obj(expected.value_, real.value_);
}

namespace sb {
namespace tests {
namespace updateserver {

class TestUpsMutator : public ::testing::Test {
 public:
  virtual void SetUp() {

  }

  virtual void TearDown() {

  }
};

TEST_F(TestUpsMutator, test_api_mutator) {
  int err = OB_SUCCESS;

  ObUpsMutator ups_mutator;

  ObCellInfo cell_info;
  ObString table_name;
  table_name.assign("sb_table", strlen("sb_table"));
  ObString row_key;
  row_key.assign("row_key", strlen("row_key"));
  ObString column_name;
  column_name.assign("column_name", strlen("column_name"));
  int64_t table_id = 10;
  uint64_t column_id = 5;
  int64_t value = 100;

  // init cell infos
  cell_info.table_name_ = table_name;
  cell_info.row_key_ = row_key;
  cell_info.column_name_ = column_name;
  cell_info.value_.set_int(value);

  ObMutator mutator;
  ObMutatorCellInfo mutation;
  mutation.cell_info = cell_info;
  mutation.op_type.set_ext(ObActionFlag::OP_UPDATE);
  err = mutator.add_cell(mutation);
  EXPECT_EQ(0, err);

  char buf[1024];
  int64_t pos = 0;
  err = mutator.serialize(buf, sizeof(buf), pos);
  EXPECT_EQ(0, err);

  pos = 0;
  err = ups_mutator.get_mutator().deserialize(buf, sizeof(buf), pos);
  EXPECT_EQ(0, err);

  ObMutatorCellInfo* new_cell_info = NULL;
  err = ups_mutator.next_cell();
  EXPECT_EQ(0, err);
  err = ups_mutator.get_cell(&new_cell_info);
  EXPECT_EQ(0, err);
  EXPECT_TRUE(NULL != new_cell_info);
  check_cell_with_name(cell_info, new_cell_info->cell_info);
  err = ups_mutator.next_cell();
  EXPECT_EQ(OB_ITER_END, err);

  new_cell_info->cell_info.column_id_ = column_id;
  new_cell_info->cell_info.table_id_ = table_id;
  new_cell_info->cell_info.column_name_.assign(NULL, 0);
  new_cell_info->cell_info.table_name_.assign(NULL, 0);

  cell_info.table_id_ = table_id;
  cell_info.column_id_ = column_id;
  ups_mutator.reset_iter();
  err = ups_mutator.next_cell();
  EXPECT_EQ(0, err);
  err = ups_mutator.get_cell(&new_cell_info);
  EXPECT_EQ(0, err);
  EXPECT_TRUE(NULL != new_cell_info);
  check_cell(cell_info, new_cell_info->cell_info);
  err = ups_mutator.next_cell();
  EXPECT_EQ(OB_ITER_END, err);
}

TEST_F(TestUpsMutator, test_freeze_memtable) {
  int err = OB_SUCCESS;
  ObUpsMutator ups_mutator;

  ups_mutator.set_freeze_memtable();

  char buf[1024];
  int64_t pos = 0;
  err = ups_mutator.serialize(buf, sizeof(buf), pos);
  EXPECT_EQ(0, err);

  ObUpsMutator new_ups_mutator;
  pos = 0;
  new_ups_mutator.deserialize(buf, sizeof(buf), pos);
  EXPECT_TRUE(new_ups_mutator.is_freeze_memtable());
}

TEST_F(TestUpsMutator, test_drop_memtable) {
  int err = OB_SUCCESS;
  ObUpsMutator ups_mutator;

  ups_mutator.set_drop_memtable();

  char buf[1024];
  int64_t pos = 0;
  err = ups_mutator.serialize(buf, sizeof(buf), pos);
  EXPECT_EQ(0, err);

  ObUpsMutator new_ups_mutator;
  pos = 0;
  new_ups_mutator.deserialize(buf, sizeof(buf), pos);
  EXPECT_TRUE(new_ups_mutator.is_drop_memtable());
}

} // end namespace updateserver
} // end namespace tests
} // end namespace sb


int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}



