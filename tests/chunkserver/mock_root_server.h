/**
 * (C) 2010 Taobao Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * mock_root_server.h
 *
 * Authors:
 *   chuanhui <rizhao.ych@taobao.com>
 *   huating <huating.zmq@taobao.com>
 *
 */
#ifndef __OCEANBASE_CHUNKSERVER_MOCK_ROOT_SERVER_H__
#define __OCEANBASE_CHUNKSERVER_MOCK_ROOT_SERVER_H__

#include <gtest/gtest.h>
#include "tbnet.h"
#include "tbsys.h"

#include "common/ob_packet_factory.h"
#include "common/ob_base_server.h"
#include "common/ob_single_server.h"
#include "common/ob_malloc.h"
#include "common/ob_result.h"
#include "common/ob_client_manager.h"
#include "common/ob_tablet_info.h"
#include "chunkserver/ob_chunk_server.h"

using namespace sb::common;
using namespace sb::chunkserver;

const int32_t MOCK_SERVER_LISTEN_PORT = 8866;

namespace sb {
namespace tests {
namespace chunkserver {
class MockRootServer : public ObSingleServer {
 public:
  MockRootServer() {
  }
  virtual ~MockRootServer() {
  }
  virtual int initialize() {
    set_batch_process(false);
    set_listen_port(MOCK_SERVER_LISTEN_PORT);
    set_dev_name("bond0");
    set_packet_factory(&factory_);
    set_default_queue_size(100);
    set_thread_count(1);
    set_packet_factory(&factory_);
    client_manager_.initialize(get_transport(), get_packet_streamer());
    ObSingleServer::initialize();

    return OB_SUCCESS;
  }

  virtual int do_request(ObPacket* base_packet) {
    int ret = OB_SUCCESS;
    ObPacket* ob_packet = base_packet;
    int32_t packet_code = ob_packet->get_packet_code();
    int32_t version = ob_packet->get_api_version();
    int32_t channel_id = ob_packet->getChannelId();
    ret = ob_packet->deserialize();

    TBSYS_LOG(INFO, "recv packet with packet_code[%d] version[%d] channel_id[%d]",
              packet_code, version, channel_id);

    if (OB_SUCCESS == ret) {
      switch (packet_code) {
      case OB_HEARTBEAT:
        handle_heartbeat(ob_packet);
        break;
      case OB_REPORT_TABLETS:
        handle_report_tablets(ob_packet);
        break;
      case OB_WAITING_JOB_DONE:
        handle_waiting_job_done(ob_packet);
        break;
      default:
        break;
      };
    }

    return ret;
  }

  int handle_heartbeat(ObPacket* packet) {
    return OB_SUCCESS;
  }

  int handle_report_tablets(ObPacket* ob_packet) {
    int ret = OB_SUCCESS;

    ObDataBuffer* data = ob_packet->get_buffer();
    if (NULL == data) {
      TBSYS_LOG(ERROR, "data is NUll should not reach this");
      ret = OB_ERROR;
    } else {
      ObResultCode result_msg;
      ObServer server;
      ObTabletReportInfoList tablet_list;
      int64_t time_stamp = 0;

      if (OB_SUCCESS == ret) {
        ret = server.deserialize(data->get_data(), data->get_capacity(),
                                 data->get_position());
        if (ret != OB_SUCCESS) {
          TBSYS_LOG(ERROR, "server.deserialize error");
        }
      }
      if (OB_SUCCESS == ret) {
        ret = tablet_list.deserialize(data->get_data(), data->get_capacity(),
                                      data->get_position());
        if (ret != OB_SUCCESS) {
          TBSYS_LOG(ERROR, "tablet_list.deserialize error");
        }
        TBSYS_LOG(INFO, "tablets size: %ld", tablet_list.get_tablet_size());
        const ObTabletReportInfo* tablets = tablet_list.get_tablet();
        EXPECT_TRUE(NULL != tablets);
        for (int64_t i = 0; i < tablet_list.get_tablet_size(); ++i) {
          TBSYS_LOG(INFO, "the %ld-th tablet: version=%ld, range=(%.*s, %.*s]\n", i,
                    tablets[i].tablet_location_.tablet_version_,
                    tablets[i].tablet_info_.range_.start_key_.length(),
                    tablets[i].tablet_info_.range_.start_key_.ptr(),
                    tablets[i].tablet_info_.range_.end_key_.length(),
                    tablets[i].tablet_info_.range_.end_key_.ptr());
        }
      }
      if (OB_SUCCESS == ret) {
        ret = serialization::decode_vi64(data->get_data(), data->get_capacity(),
                                         data->get_position(), &time_stamp);
        if (ret != OB_SUCCESS) {
          TBSYS_LOG(ERROR, "time_stamp.deserialize error");
        }
        TBSYS_LOG(INFO, "timestamp: %ld", time_stamp);
      }

      tbnet::Connection* connection = ob_packet->get_connection();
      ThreadSpecificBuffer::Buffer* thread_buffer =
        response_packet_buffer_.get_buffer();
      if (NULL != thread_buffer) {
        thread_buffer->reset();
        ObDataBuffer out_buffer(thread_buffer->current(), thread_buffer->remain());

        result_msg.result_code_ = ret;
        result_msg.serialize(out_buffer.get_data(), out_buffer.get_capacity(),
                             out_buffer.get_position());

        TBSYS_LOG(DEBUG, "handle tablets report packet");

        int32_t version = 1;
        int32_t channel_id = ob_packet->getChannelId();
        ret = send_response(OB_REPORT_TABLETS_RESPONSE, version, out_buffer,
                            connection, channel_id);
      } else {
        TBSYS_LOG(ERROR, "get thread buffer error, ignore this packet");
        ret = OB_ERROR;
      }
    }

    return ret;
  }

  int handle_waiting_job_done(ObPacket* ob_packet) {
    int ret = OB_SUCCESS;

    ObDataBuffer* data_buffer = ob_packet->get_buffer();
    if (NULL == data_buffer) {
      TBSYS_LOG(ERROR, "data_buffer is NUll should not reach this");
      ret = OB_ERROR;
    } else {
      tbnet::Connection* connection = ob_packet->get_connection();
      ThreadSpecificBuffer::Buffer* thread_buffer =
        response_packet_buffer_.get_buffer();
      if (NULL != thread_buffer) {
        thread_buffer->reset();
        ObDataBuffer out_buffer(thread_buffer->current(), thread_buffer->remain());

        ObResultCode response;
        response.result_code_ = OB_SUCCESS;
        response.serialize(out_buffer.get_data(), out_buffer.get_capacity(),
                           out_buffer.get_position());

        TBSYS_LOG(DEBUG, "handle wait job done");

        int32_t version = 1;
        int32_t channel_id = ob_packet->getChannelId();
        ret = send_response(OB_WAITING_JOB_DONE_RESPONSE, version, out_buffer,
                            connection, channel_id);
        TBSYS_LOG(DEBUG, "handle wait job done");
      } else {
        TBSYS_LOG(ERROR, "get thread buffer error, ignore this packet");
        ret = OB_ERROR;
      }
    }

    return ret;
  }

 private:
  ObPacketFactory factory_;
  ObClientManager  client_manager_;
  ThreadSpecificBuffer response_packet_buffer_;
};

class MockRootServerRunner : public tbsys::Runnable {
 public:
  virtual void run(tbsys::CThread* thread, void* arg) {
    MockRootServer mock_server;
    mock_server.start();
  }
};
}
}
}

#endif //__MOCK_ROOT_SERVER_H__

