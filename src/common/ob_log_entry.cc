/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * ob_log_entry.cc for ...
 *
 * Authors:
 *   yanran <yanran.hfs@taobao.com>
 *
 */
#include "ob_log_entry.h"
#include "ob_crc64.h"
#include "utility.h"

using namespace sb::common;

const char* sb::common::DEFAULT_CKPT_EXTENSION = "checkpoint";

int ObLogEntry::fill_header(const char* log_data, const int64_t data_len) {
  int ret = OB_SUCCESS;

  if ((NULL == log_data && data_len != 0)
      || (NULL != log_data && data_len <= 0)) {
    ret = OB_INVALID_ARGUMENT;
  } else {
    header_.set_magic_num(MAGIC_NUMER);
    header_.header_length_ = OB_RECORD_HEADER_LENGTH;
    header_.version_ = LOG_VERSION;
    header_.reserved_ = 0;
    header_.data_length_ = sizeof(uint64_t) + sizeof(LogCommand) + data_len;
    header_.data_zlength_ = header_.data_length_;
    if (NULL != log_data) {
      header_.data_checksum_ = calc_data_checksum(log_data, data_len);
    } else {
      header_.data_checksum_ = 0;
    }
    header_.set_header_checksum();
  }

  return ret;
}

int64_t ObLogEntry::calc_data_checksum(const char* log_data, const int64_t data_len) const {
  uint64_t data_checksum = 0;

  if (data_len > 0) {
    data_checksum = ob_crc64(reinterpret_cast<const void*>(&seq_), sizeof(seq_));
    data_checksum = ob_crc64(data_checksum, reinterpret_cast<const void*>(&cmd_), sizeof(cmd_));
    data_checksum = ob_crc64(data_checksum, reinterpret_cast<const void*>(log_data), data_len);
  }

  return static_cast<int64_t>(data_checksum);
}

int ObLogEntry::check_header_integrity() const {
  int ret = OB_SUCCESS;
  if (OB_SUCCESS == header_.check_header_checksum()
      && OB_SUCCESS == header_.check_magic_num(MAGIC_NUMER)) {
    ret = OB_SUCCESS;
  } else {
    TBSYS_LOG(WARN, "check_header_integrity error: ");
    hex_dump(&header_, sizeof(header_), true, TBSYS_LOG_LEVEL_WARN);
  }

  return ret;
}

int ObLogEntry::check_data_integrity(const char* log_data) const {
  int ret = OB_SUCCESS;

  if (log_data == NULL) {
    ret = OB_INVALID_ARGUMENT;
  } else {
    int64_t crc_check_sum = calc_data_checksum(log_data, header_.data_length_ - sizeof(uint64_t) - sizeof(LogCommand));
    if ((OB_SUCCESS == header_.check_header_checksum())
        && (OB_SUCCESS == header_.check_magic_num(MAGIC_NUMER))
        && (crc_check_sum == header_.data_checksum_)) {
      ret = OB_SUCCESS;
    } else {
      TBSYS_LOG(WARN, "Header: ");
      hex_dump(&header_, sizeof(header_), true, TBSYS_LOG_LEVEL_WARN);
      TBSYS_LOG(WARN, "Body: ");
      hex_dump(log_data - sizeof(uint64_t) - sizeof(LogCommand), header_.data_length_, true, TBSYS_LOG_LEVEL_WARN);
      ret = OB_ERROR;
    }
  }

  return ret;
}

DEFINE_SERIALIZE(ObLogEntry) {
  int ret = OB_SUCCESS;
  int64_t serialize_size = get_serialize_size();
  if (NULL == buf || (pos + serialize_size) > buf_len) {
    ret = OB_ERROR;
  } else {
    int64_t new_pos = pos;
    if ((OB_SUCCESS == header_.serialize(buf, buf_len, new_pos))
        && (OB_SUCCESS == serialization::encode_i64(buf, buf_len, new_pos, seq_))
        && (OB_SUCCESS == serialization::encode_i32(buf, buf_len, new_pos, cmd_))) {
      pos = new_pos;
    } else {
      ret = OB_ERROR;
    }
  }

  return ret;
}

DEFINE_DESERIALIZE(ObLogEntry) {
  int ret = OB_SUCCESS;

  int64_t serialize_size = get_serialize_size();
  if (NULL == buf || (serialize_size > data_len - pos)) {
    ret = OB_ERROR;
  } else {
    int64_t new_pos = pos;
    if ((OB_SUCCESS == header_.deserialize(buf, data_len, new_pos))
        && (OB_SUCCESS == serialization::decode_i64(buf, data_len, new_pos, (int64_t*)&seq_))
        && (OB_SUCCESS == serialization::decode_i32(buf, data_len, new_pos, (int32_t*)&cmd_))) {
      pos = new_pos;
    } else {
      ret = OB_ERROR;
    }
  }

  return ret;
}

DEFINE_GET_SERIALIZE_SIZE(ObLogEntry) {
  return header_.get_serialize_size()
         + serialization::encoded_length_i64(seq_)
         + serialization::encoded_length_i32(cmd_);
}


