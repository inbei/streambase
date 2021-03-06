/**
 * (C) 2010 Taobao Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * ob_read_worker.h for define read worker thread.
 *
 * Authors:
 *   huating <huating.zmq@taobao.com>
 *
 */
#ifndef OCEANBASE_SYSCHECKER_READ_WORKER_H_
#define OCEANBASE_SYSCHECKER_READ_WORKER_H_

#include <tbsys.h>
#include "client/ob_client.h"
#include "ob_syschecker_rule.h"
#include "ob_syschecker_stat.h"

namespace sb {
namespace syschecker {
class ObReadWorker : public tbsys::CDefaultRunnable {
 public:
  ObReadWorker(client::ObClient& client, ObSyscheckerRule& rule,
               ObSyscheckerStat& stat);
  ~ObReadWorker();

 public:
  virtual void run(tbsys::CThread* thread, void* arg);

 private:
  int init(ObOpParam** read_param);
  int run_get(ObOpParam& read_param, common::ObGetParam& get_param,
              common::ObScanner& scanner);
  int run_scan(ObOpParam& read_param, common::ObScanParam& scan_param,
               common::ObScanner& scanner);
  int check_scanner_result(const ObOpParam& read_param,
                           const common::ObScanner& scanner,
                           const bool is_get);

  int check_cell(const ObOpCellParam& cell_param,
                 const common::ObCellInfo& cell_info);
  int check_null_cell(const int64_t prefix,
                      const common::ObCellInfo& cell_info,
                      const common::ObCellInfo& aux_cell_info);
  int check_cell(const ObOpCellParam& cell_param,
                 const ObOpCellParam& aux_cell_param,
                 const common::ObCellInfo& cell_info,
                 const common::ObCellInfo& aux_cell_info);
  int display_scanner(const common::ObScanner& scanner) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(ObReadWorker);

  client::ObClient& client_;
  ObSyscheckerRule& read_rule_;
  ObSyscheckerStat& stat_;
};
} // end namespace syschecker
} // end namespace sb

#endif //OCEANBASE_SYSCHECKER_READ_WORKER_H_
