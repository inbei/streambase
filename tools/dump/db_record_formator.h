/*
 * =====================================================================================
 *
 *       Filename:  db_record_formator.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  06/17/2011 01:07:03 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  yushun.swh (), yushun.swh@taobao.com
 *        Company:  taobao
 *
 * =====================================================================================
 */

#ifndef  DB_RECORD_FORMATOR_INC
#define  DB_RECORD_FORMATOR_INC

#include "db_record.h"
#include "common/ob_action_flag.h"
#include <string>

using namespace sb::common;
using namespace sb::api;

class FormatorHeader {
 public:
  virtual ~FormatorHeader() { }
  virtual int append_header(const ObString& rowkey, int op,
                            int64_t seq, std::string& app_name,
                            std::string& table_name, ObDataBuffer& buffer) = 0;
};

class UniqFormatorHeader : public FormatorHeader {
 public:
  virtual int append_header(const ObString& rowkey, int op, int64_t seq, std::string& app_name,
                            std::string& table_name, ObDataBuffer& buffer);
};

class DbRecordFormator {
 public:

 public:
  DbRecordFormator() { }
  virtual ~DbRecordFormator() { }

  virtual int format_record(int64_t table_id, DbRecord* record,
                            const ObString& rowkey, ObDataBuffer& buffer);
};

#endif   /* ----- #ifndef db_record_formator_INC  ----- */
