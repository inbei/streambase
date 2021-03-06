/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * ob_row_cell_vec.cc for ...
 *
 * Authors:
 *   wushi <wushi.ly@taobao.com>
 *
 */
#include "ob_row_cell_vec.h"
#include <errno.h>
#include "common/ob_malloc.h"
#include "common/ob_action_flag.h"
using namespace sb;
using namespace sb::common;
using namespace sb::mergeserver;

sb::mergeserver::ObRowCellVec::ObRowCellVec() {
  row_buffer_ = NULL;
  row_buffer_size_ = 0;
  cur_buffer_offset_ = 0;
  row_ = NULL;
  table_id_ = OB_INVALID_ID;
  consumed_cell_num_ = -1;
  is_empty_ = true;
}

sb::mergeserver::ObRowCellVec::~ObRowCellVec() {
  ob_free(row_buffer_);
  row_buffer_ = NULL;
  row_buffer_size_ = 0;
  cur_buffer_offset_ = 0;
  row_ = NULL;
  table_id_ = OB_INVALID_ID;
  consumed_cell_num_ = -1;
  is_empty_ = true;
}


bool sb::mergeserver::ObRowCellVec::is_empty() {
  return is_empty_;
}

int sb::mergeserver::ObRowCellVec::init() {
  int err  = OB_SUCCESS;
  if (NULL == row_buffer_) {
    row_buffer_ = reinterpret_cast<char*>(ob_malloc(OB_MAX_PACKET_LENGTH, ObModIds::OB_MS_JOIN_CACHE));
    if (NULL != row_buffer_) {
      row_buffer_size_ = OB_MAX_PACKET_LENGTH;
    }
  }
  if (NULL == row_buffer_) {
    TBSYS_LOG(WARN, "fail to allocate buffer [errno:%d]", errno);
    err = OB_ALLOCATE_MEMORY_FAILED;
  }
  if (OB_SUCCESS == err) {
    row_ = reinterpret_cast<ObRow*>(row_buffer_);
    cur_buffer_offset_ = sizeof(ObRow);
    row_->cell_num_ = 0;
    row_->row_key_len_ = 0;
    row_key_.assign(NULL, 0);
    table_id_ = OB_INVALID_ID;
    consumed_cell_num_ = -1;
    is_empty_ = true;
  }
  return err;
}

int sb::mergeserver::ObRowCellVec::add_cell(const ObCellInfo& cell) {
  int err = OB_SUCCESS;
  if (NULL == row_) {
    err = OB_INVALID_ARGUMENT;
    TBSYS_LOG(WARN, "row not initialized");
  }
  if (OB_SUCCESS == err && row_key_.length() == 0) {
    row_key_ = cell.row_key_;
    row_->row_key_len_ = row_key_.length();
    table_id_ = cell.table_id_;
  }
  if (OB_SUCCESS == err && (row_key_ != cell.row_key_ || table_id_ != cell.table_id_)) {
    TBSYS_LOG(WARN, "row key or tableid not coincident [row_key_:%.*s,cell.row_key_:%.*s,"
              "table_id_:%lu,cell.table_id_:%lu]",
              row_key_.length(), row_key_.ptr(), cell.row_key_.length(), cell.row_key_.ptr(),
              table_id_, cell.table_id_);
    err = OB_INVALID_ARGUMENT;
  }
  if (OB_SUCCESS == err) {
    bool add_cur_cell = false;
    if (cell.value_.get_ext() == ObActionFlag::OP_DEL_ROW
        || cell.value_.get_ext() == ObActionFlag::OP_DEL_TABLE) {
      row_->cell_num_ = 0;
      cur_buffer_offset_ = sizeof(ObRow);
      add_cur_cell = true;
    } else if (cell.value_.get_ext() == ObActionFlag::OP_NOP) {
      if (0 == row_->cell_num_) {
        add_cur_cell = true;
      }
    } else {
      int32_t cell_idx = 0;
      for (; cell_idx < row_->cell_num_; cell_idx ++) {
        if (row_->cells_[cell_idx].column_id_ == cell.column_id_) {
          err = row_->cells_[cell_idx].value_.apply(cell.value_);
          break;
        }
      }
      if (OB_SUCCESS == err && cell_idx >= row_->cell_num_) {
        add_cur_cell = true;
      }
    }
    if (add_cur_cell) {
      if (row_buffer_size_ - cur_buffer_offset_ < static_cast<int64_t>(sizeof(ObMsCellInfo))) {
        err = OB_BUF_NOT_ENOUGH;
      } else {
        ObMsCellInfo ms_cell;
        ms_cell.column_id_ = cell.column_id_;
        ms_cell.value_ = cell.value_;
        memcpy(row_buffer_ + cur_buffer_offset_, &(ms_cell), sizeof(ObMsCellInfo));
        cur_buffer_offset_ += sizeof(ObMsCellInfo);
        if (cell.value_.get_ext() == ObActionFlag::OP_DEL_ROW
            || cell.value_.get_ext() == ObActionFlag::OP_DEL_TABLE
            || cell.value_.get_ext() == ObActionFlag::OP_ROW_DOES_NOT_EXIST) {
          row_->cells_[row_->cell_num_].column_id_ = OB_INVALID_ID;
        }
        row_->cell_num_ ++;
      }
    }
  }
  if (OB_SUCCESS == err) {
    is_empty_ = false;
  }
  return err;
}



void sb::mergeserver::ObRowCellVec::reset_iterator() {
  consumed_cell_num_ = -1;
}

int sb::mergeserver::ObRowCellVec::next_cell() {
  int err = OB_SUCCESS;
  if (NULL == row_ || 0 == row_->cell_num_) {
    err = OB_ITER_END;
    TBSYS_LOG(WARN, "row not initialized");
  }
  if (OB_SUCCESS == err) {
    consumed_cell_num_ ++;
    if (consumed_cell_num_ >= row_->cell_num_) {
      err = OB_ITER_END;
    }
  }
  return err;
}

int sb::mergeserver::ObRowCellVec::get_cell(ObCellInfo** cell, bool* is_row_changed) {
  int err = OB_SUCCESS;
  if (NULL == row_ || 0 == row_->cell_num_) {
    err = OB_ITER_END;
    TBSYS_LOG(WARN, "row not initialized");
  }
  if (OB_SUCCESS == err && consumed_cell_num_ >= row_->cell_num_) {
    err = OB_ITER_END;
  }
  if (OB_SUCCESS == err) {
    if (NULL != cell) {
      cur_cell_.row_key_ = row_key_;
      cur_cell_.table_id_ = row_->table_id_;
      cur_cell_.value_ = row_->cells_[consumed_cell_num_].value_;
      cur_cell_.column_id_ = row_->cells_[consumed_cell_num_].column_id_;
      /// *cell = row_->cells_ + consumed_cell_num_;
      *cell = &cur_cell_;
    }
    if (NULL != is_row_changed) {
      if (0 == consumed_cell_num_) {
        *is_row_changed = true;
      } else {
        *is_row_changed = false;
      }
    }
  }
  return err;
}


int sb::mergeserver::ObRowCellVec::get_cell(ObCellInfo** cell) {
  return get_cell(cell, NULL);
}

int64_t sb::mergeserver::ObRowCellVec::get_serialize_size()const {
  int64_t res = 0;
  int64_t err = OB_SUCCESS;
  if (NULL == row_ || 0 == row_->cell_num_) {
    err = OB_INVALID_ARGUMENT;
    TBSYS_LOG(WARN, "row not initialized");
  }
  if (OB_SUCCESS == err) {
    ObString value;
    res += sizeof(ObRow) + sizeof(ObMsCellInfo) * row_->cell_num_ + row_key_.length();
    for (int32_t i = 0; i < row_->cell_num_ && OB_SUCCESS == err; i++) {
      if (row_->cells_[i].value_.get_type() == ObVarcharType) {
        err = row_->cells_[i].value_.get_varchar(value);
        if (OB_SUCCESS == err) {
          res += value.length();
        } else {
          TBSYS_LOG(WARN, "unexpected error, get varchar [err:%d]", err);
        }
      }
    }
  }
  if (OB_SUCCESS != err) {
    res = 0;
  }
  return res;
}


int sb::mergeserver::ObRowCellVec::serialize(char* buf, const int64_t buf_len,
                                             int64_t& pos)const {
  int err = OB_SUCCESS;
  ObRow* serialized_row = NULL;
  int64_t cur_item_len = 0;
  int64_t org_pos = pos;
  if (NULL == row_ || 0 == row_->cell_num_) {
    err = OB_INVALID_ARGUMENT;
    TBSYS_LOG(WARN, "row not initialized");
  } else {
    cur_item_len = sizeof(ObRow) + sizeof(ObMsCellInfo) * row_->cell_num_;
  }

  if (OB_SUCCESS == err
      && (NULL == buf || buf_len <= pos || buf_len - pos < cur_item_len + row_key_.length())) {
    err = OB_BUF_NOT_ENOUGH;
  }
  /// copy cells
  if (OB_SUCCESS == err) {
    row_->table_id_ = table_id_;
    if (buf_len - pos >= cur_item_len) {
      memcpy(buf + pos, row_, cur_item_len);
      serialized_row = reinterpret_cast<ObRow*>(buf + pos);
      pos += cur_item_len;
    } else {
      err = OB_INVALID_ARGUMENT;
    }
  }
  /// copy rowkey
  if (OB_SUCCESS == err) {
    cur_item_len = row_key_.length();
    if (buf_len - pos >= cur_item_len) {
      memcpy(buf + pos, row_key_.ptr(), cur_item_len);
      pos += cur_item_len;
    } else {
      err = OB_INVALID_ARGUMENT;
    }
  }
  /// copy values
  if (OB_SUCCESS == err) {
    ObString cur_value;
    for (int32_t i = 0; i < row_->cell_num_ && OB_SUCCESS == err; i++) {
      if (row_->cells_[i].value_.get_type() == ObVarcharType) {
        err = row_->cells_[i].value_.get_varchar(cur_value);
        if (OB_SUCCESS == err) {
          cur_item_len = cur_value.length();
          if (buf_len - pos >= cur_item_len) {
            memcpy(buf + pos, cur_value.ptr(), cur_item_len);
            cur_value.assign(reinterpret_cast<char*>(pos - org_pos), cur_item_len);
            serialized_row->cells_[i].value_.set_varchar(cur_value);
            pos += cur_item_len;
          } else {
            err = OB_INVALID_ARGUMENT;
          }
        }
      }
    }
  }
  return err;
}

int sb::mergeserver::ObRowCellVec::deserialize(const char* buf, const int64_t buf_len, int64_t& pos) {
  int err = OB_SUCCESS;
  if (NULL == row_buffer_) {
    row_buffer_ = reinterpret_cast<char*>(ob_malloc(OB_MAX_PACKET_LENGTH, ObModIds::OB_MS_JOIN_CACHE));
    if (NULL != row_buffer_) {
      row_buffer_size_ = OB_MAX_PACKET_LENGTH;
    }
  }
  if (NULL == row_buffer_) {
    TBSYS_LOG(WARN, "fail to allocate buffer [errno:%d]", errno);
    err = OB_ALLOCATE_MEMORY_FAILED;
  }
  if (OB_SUCCESS == err) {
    if (NULL == buf || pos >= buf_len) {
      err = OB_INVALID_ARGUMENT;
      TBSYS_LOG(WARN, "invalid argument[buf:%p, buf_len:%d, pos:%d]", buf, buf_len, pos);
    }
  }
  if (OB_SUCCESS == err && buf_len - pos > row_buffer_size_) {
    err = OB_BUF_NOT_ENOUGH;
  }
  if (OB_SUCCESS == err) {
    consumed_cell_num_ = -1;
    table_id_ = OB_INVALID_ID;
    is_empty_ = true;
  }
  if (OB_SUCCESS == err) {
    cur_buffer_offset_ = buf_len - pos;
    memcpy(row_buffer_, buf + pos, cur_buffer_offset_);
    row_ = reinterpret_cast<ObRow*>(row_buffer_);
    row_key_.assign(row_buffer_ + sizeof(ObRow) + row_->cell_num_ * sizeof(ObMsCellInfo), row_->row_key_len_);
    table_id_ = row_->table_id_;
  }
  if (OB_SUCCESS == err) {
    ObString cur_value;
    int64_t cur_offset = 0;
    int64_t cur_len = 0;
    for (int i = 0; i < row_->cell_num_ && OB_SUCCESS == err; i++) {
      if (row_->cells_[i].value_.get_type() == ObVarcharType) {
        err = row_->cells_[i].value_.get_varchar(cur_value);
        if (OB_SUCCESS == err) {
          cur_offset = reinterpret_cast<int64_t>(cur_value.ptr());
          cur_len = cur_value.length();
          cur_value.assign(row_buffer_ + cur_offset, cur_len);
          row_->cells_[i].value_.set_varchar(cur_value);
        }
      }
    }
  }

  if (OB_SUCCESS == err && 0 < row_->cell_num_) {
    is_empty_ = false;
  }
  return err;
}


