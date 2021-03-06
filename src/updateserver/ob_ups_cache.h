/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * ob_ups_cache.h for ...
 *
 * Authors:
 *   rongxuan <rongxuan.lc@taobao.com>
 *
 */
#ifndef  OCEANBASE_UPDATESERVER_UPSCACHE_H_
#define  OCEANBASE_UPDATEKSERVER_UPSCACHE_H_


#include <string.h>
#include "common/ob_object.h"
#include "common/ob_define.h"
#include "common/ob_kv_storecache.h"
#include "common/ob_action_flag.h"

namespace sb {
namespace updateserver {
struct ObUpsCacheKey {
  uint64_t table_id: 16;
  uint64_t column_id: 16;
  int32_t nbyte;
  char* buffer;
  ObUpsCacheKey(): table_id(0), column_id(0), nbyte(0), buffer(NULL) {
  };
  int64_t hash() const {
    int64_t hash_for_int = common::murmurhash2(this, sizeof(ObUpsCacheKey) - sizeof(char*), 0);
    int64_t hash_for_rowkey = common::murmurhash2(this->buffer, nbyte, 0);

    return (hash_for_int + hash_for_rowkey);
  };
  bool operator == (const ObUpsCacheKey& other) const {
    return (table_id == other.table_id
            && column_id == other.column_id
            && nbyte == other.nbyte
            && 0 == strncmp(buffer, other.buffer, nbyte));
  };
};
struct ObUpsCacheValue {
  int64_t version;
  common::ObObj value;
  ObUpsCacheValue() {
  };
  ObUpsCacheValue(int64_t ver, common::ObObj val) {
    version = ver;
    value = val;
  }
};
}

namespace common {
namespace KVStoreCacheComponent {
struct UPSCacheKeyDeepCopyTag {};
template <>
struct traits<updateserver::ObUpsCacheKey> {
  typedef UPSCacheKeyDeepCopyTag Tag;
};

inline updateserver::ObUpsCacheKey* do_copy(const updateserver::ObUpsCacheKey& other,
                                            char* buffer, UPSCacheKeyDeepCopyTag) {
  updateserver::ObUpsCacheKey* ret = (updateserver::ObUpsCacheKey*)buffer;
  if (NULL != ret) {
    ret->table_id = other.table_id;
    ret->column_id = other.column_id;
    ret->nbyte = other.nbyte;
    ret->buffer = buffer + sizeof(updateserver::ObUpsCacheKey);
    memcpy(ret->buffer, other.buffer, other.nbyte);
  }
  return ret;
}

inline int32_t do_size(const updateserver::ObUpsCacheKey& key, UPSCacheKeyDeepCopyTag) {
  return (sizeof(updateserver::ObUpsCacheKey) + key.nbyte);
}

inline void do_destroy(updateserver::ObUpsCacheKey* key, UPSCacheKeyDeepCopyTag) {
  UNUSED(key);
}

struct ObUpsCacheValueDeepCopyTag {};

template <>
struct traits<updateserver::ObUpsCacheValue> {
  typedef ObUpsCacheValueDeepCopyTag Tag;
};

inline updateserver::ObUpsCacheValue* do_copy(const updateserver::ObUpsCacheValue& other,
                                              char* buffer, ObUpsCacheValueDeepCopyTag) {
  //if the type_ == varchar ,do the deep copy
  updateserver::ObUpsCacheValue* ret = new(buffer)updateserver::ObUpsCacheValue(other);
  char* buf = buffer;
  if (ObVarcharType == other.value.get_type()) {
    common::ObString value;
    other.value.get_varchar(value);
    buf = buf + sizeof(updateserver::ObUpsCacheValue);
    memcpy(buf, value.ptr(), value.length());
    common::ObString new_string(value.size(), value.length(), buf);
    ret->value.set_varchar(new_string);
  } else {
    //do nothing;
  }

  return ret;
}

inline int32_t do_size(const updateserver::ObUpsCacheValue& value, ObUpsCacheValueDeepCopyTag) {
  common::ObString strVal;
  value.value.get_varchar(strVal);
  int32_t length = strVal.length();
  return (sizeof(updateserver::ObUpsCacheValue) + length);
}

inline void do_destroy(updateserver::ObUpsCacheValue* value, ObUpsCacheValueDeepCopyTag) {
  UNUSED(value);
}
}
}

namespace updateserver {
namespace test {
class TestBlockCacheHelper;
}
class ObBufferHandle;
//class TestBlockCache;
class ObUpsCache {
 public:
  friend class ObBufferHandle;
  friend class test::TestBlockCacheHelper;
  static const int64_t MIN_KVCACHE_SIZE = 1024L * 1024L * 500L; //500M
  static const int64_t KVCACHE_ITEM_SIZE = 128;
  static const int64_t KVCACHE_BLOCK_SIZE = 1024L * 1024L; //1M
  typedef common::KeyValueCache<ObUpsCacheKey, ObUpsCacheValue, KVCACHE_ITEM_SIZE, KVCACHE_BLOCK_SIZE> KVCache;
  typedef common::CacheHandle Handle;
 public:
  ObUpsCache();
  ~ObUpsCache();

  int init(const int64_t cache_size, const int64_t max_cache_size_limit);
  int destroy();
  // 获取某个缓存项
  // 如果缓存项存在，返回OB_SUCCESS; 如果缓存项不存在，返回OB_NOT_EXIST；否则，返回OB_ERROR
  int get(const uint64_t table_id,
          const common::ObString& row_key, ObBufferHandle& buffer_handle,
          const uint64_t column_id, updateserver::ObUpsCacheValue& value);

  // 加入缓存项
  int put(const uint64_t table_id,
          const common::ObString& row_key,
          const uint64_t column_id,
          const updateserver::ObUpsCacheValue& value);

  // 查询行是否存在
  //在缓存中column_id = 0表明是存在行查询
  //在预处理线程中，obgetparam中如果要进行存在行查询时，column_id是等于ob_is_row_exist.
  //在查询缓存是需要转换为0的。
  // 如果缓存项存在，返回OB_SUCCESS; 如果缓存项不存在，返回OB_NOT_EXIST；否则，返回OB_ERROR；
  int is_row_exist(const uint64_t table_id,
                   const common::ObString& row_key,
                   bool& is_exist,
                   ObBufferHandle& buffer_handle);

  // 设置行是否存在标志，行不存在也需要记录到缓存中
  int set_row_exist(const uint64_t table_id, const common::ObString& row_key, const bool is_exist);

 private:

  // 设置缓存大小限制，单位为字节
  void set_cache_size_limit(const int64_t cache_size);

  //设置缓存占用内存，单位为字节
  void set_cache_size(const int64_t cache_size);

 public:

  // 获取缓存大小限制，单位为字节
  int64_t get_cache_size_limit(void);

  // 获取缓存占用的内存，单位为字节
  int64_t get_cache_size(void);


  int clear();

 private:
  bool inited_;
  KVCache kv_cache_;
  int64_t max_cache_size_limit_;
  int64_t cache_size_;
};

class ObBufferHandle {
 public:
  friend class ObUpsCache;
  ObBufferHandle(): ups_cache_(NULL), buffer_(NULL) {
  }

  explicit ObBufferHandle(char* buffer): ups_cache_(NULL), buffer_(buffer) {
  }

  ~ObBufferHandle() {
    if (NULL != ups_cache_) {
      ups_cache_->kv_cache_.revert(handle_);
    }
    ups_cache_ = NULL;
    buffer_ = NULL;
  }

  explicit ObBufferHandle(const ObBufferHandle& other) {
    *this = other;
  }

  const char* get_buffer() const {
    return buffer_;
  }

  ObBufferHandle& operator=(const ObBufferHandle& other) {
    ObUpsCache* ups_cache_temp = ups_cache_;
    ObUpsCache::Handle handle_temp = handle_;

    if (NULL != other.ups_cache_) {
      if (common::OB_SUCCESS == other.ups_cache_->kv_cache_.dup_handle(other.handle_, handle_)) {
        ups_cache_ = other.ups_cache_;
        buffer_ = other.buffer_;
      } else {
        TBSYS_LOG(ERROR, "copy handle fail this ups_cache=%p this buffer=%p other_ups_cache=%p other_buffer=%p", ups_cache_, buffer_, other.ups_cache_, other.buffer_);
        ups_cache_ = NULL;
        buffer_ = NULL;
      }
    } else {
      ups_cache_ = other.ups_cache_;
      buffer_ = other.buffer_;
    }

    if (NULL != ups_cache_temp) {
      ups_cache_temp->kv_cache_.revert(handle_temp);
    }

    return *this;
  }

 private:
  ObUpsCache* ups_cache_;
  ObUpsCache::Handle handle_;
  char* buffer_;
};
}
}

#endif //OCEANBAST_UPDATESERVER_UPSCACHE_H


