/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * test_merger_rpc_stub.cc for ...
 *
 * Authors:
 *   xielun <xielun.szd@taobao.com>
 *
 */
#include <iostream>
#include <sstream>
#include <algorithm>
#include <tblog.h>
#include <gtest/gtest.h>

#include "common/ob_schema.h"
#include "common/ob_malloc.h"
#include "common/ob_scanner.h"
#include "common/ob_tablet_info.h"
#include "ob_ms_rpc_stub.h"
#include "ob_ms_tablet_location_item.h"
#include "mock_server.h"
#include "mock_root_server.h"
#include "mock_update_server.h"
#include "mock_chunk_server.h"

using namespace std;
using namespace sb::common;
using namespace sb::mergeserver;
using namespace sb::mergeserver::test;

const uint64_t timeout = 100000;
const char* addr = "localhost";

int main(int argc, char** argv) {
  ob_init_memory_pool();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

class TestRpcStub: public ::testing::Test {
 public:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

TEST_F(TestRpcStub, test_init) {
  ObMergerRpcStub stub;
  EXPECT_TRUE(OB_SUCCESS != stub.init(NULL, NULL));

  ThreadSpecificBuffer buffer;
  ObClientManager client_manager;
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  EXPECT_TRUE(OB_SUCCESS != stub.init(&buffer, &client_manager));
}

TEST_F(TestRpcStub, test_find_server) {
  // client thread one
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  // 2 & 3 (io transport thread)
  transport.start();
  ObClientManager client_manager;
  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  ObServer name_server;
  name_server.set_ipv4_addr(addr, MockRootServer::ROOT_SERVER_PORT);

  // register error
  ObServer update_server;
  EXPECT_TRUE(OB_SUCCESS != stub.find_server(timeout, name_server, update_server));

  // server thread
  MockRootServer server;
  MockServerRunner test_root_server(server);
  tbsys::CThread root_server_thread;
  // 4(queue) & 5 & 6(io transport) & 7(main)
  root_server_thread.start(&test_root_server, NULL);
  sleep(2);

  EXPECT_TRUE(OB_SUCCESS == stub.find_server(timeout, name_server, update_server));
  EXPECT_TRUE(MockUpdateServer::UPDATE_SERVER_PORT == update_server.get_port());

  EXPECT_TRUE(OB_SUCCESS == stub.find_server(timeout, name_server, update_server));
  EXPECT_TRUE(MockUpdateServer::UPDATE_SERVER_PORT == update_server.get_port());

  transport.stop();
  server.stop();
  sleep(10);
}

TEST_F(TestRpcStub, test_register_server) {
  // client thread one
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  // 2 & 3 (io transport thread)
  transport.start();
  ObClientManager client_manager;
  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  // self server
  ObServer merge_server;
  merge_server.set_ipv4_addr(addr, 12356);

  ObServer name_server;
  name_server.set_ipv4_addr(addr, MockRootServer::ROOT_SERVER_PORT);

  // register error
  EXPECT_TRUE(OB_SUCCESS != stub.register_server(timeout, name_server, merge_server, true));

  // server thread
  MockRootServer server;
  MockServerRunner test_root_server(server);
  tbsys::CThread root_server_thread;
  // 4(queue) & 5 & 6(io transport) & 7(main)
  root_server_thread.start(&test_root_server, NULL);
  sleep(2);

  EXPECT_TRUE(OB_SUCCESS == stub.register_server(timeout, name_server, merge_server, true));
  EXPECT_TRUE(OB_SUCCESS == stub.register_server(timeout, name_server, merge_server, true));

  transport.stop();
  server.stop();
  sleep(10);
}

TEST_F(TestRpcStub, test_scan_root_table) {
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  transport.start();
  ObClientManager client_manager;

  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  // self server
  ObServer name_server;
  name_server.set_ipv4_addr(addr, MockRootServer::ROOT_SERVER_PORT);

  // scan root error
  char* string = "test_table";
  uint64_t name_table = 0;
  uint64_t table_id = 100;
  ObString row_key;
  row_key.assign(string, strlen(string));
  ObScanner scanner;
  EXPECT_TRUE(OB_SUCCESS != stub.fetch_tablet_location(timeout, name_server, name_table, table_id, row_key, scanner));

  // start root server
  MockRootServer server;
  MockServerRunner test_root_server(server);
  tbsys::CThread root_server_thread;
  root_server_thread.start(&test_root_server, NULL);
  sleep(2);

  // root table id = 0
  name_table = 0;
  EXPECT_TRUE(OB_SUCCESS == stub.fetch_tablet_location(timeout, name_server, name_table, table_id, row_key, scanner));

  transport.stop();
  server.stop();
  sleep(10);
}

TEST_F(TestRpcStub, test_fetch_schema) {
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  transport.start();
  ObClientManager client_manager;

  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  // self server
  ObServer name_server;
  name_server.set_ipv4_addr(addr, MockRootServer::ROOT_SERVER_PORT);

  // start root server
  MockRootServer server;
  MockServerRunner test_root_server(server);
  tbsys::CThread root_server_thread;
  root_server_thread.start(&test_root_server, NULL);
  sleep(2);

  ObSchemaManagerV2 manager;
  // wrong version
  int64_t timestamp = 1023;
  EXPECT_TRUE(OB_SUCCESS != stub.fetch_schema(timeout, name_server, timestamp, manager));

  timestamp = 1024;
  EXPECT_TRUE(OB_SUCCESS == stub.fetch_schema(timeout, name_server, timestamp, manager));
  EXPECT_TRUE(manager.get_version() == 1025);

  transport.stop();
  server.stop();
  sleep(10);
}

TEST_F(TestRpcStub, test_get_servers) {
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  transport.start();
  ObClientManager client_manager;

  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  // self server
  ObServer chunk_server;
  chunk_server.set_ipv4_addr(addr, MockChunkServer::CHUNK_SERVER_PORT);

  ObMergerTabletLocationList list;
  ObTabletLocation addr;
  //addr.tablet_id_ = 100;
  addr.chunkserver_ = chunk_server;

  list.add(addr);
  list.add(addr);
  list.add(addr);

  // start root server
  MockChunkServer server;
  MockServerRunner test_chunk_server(server);
  tbsys::CThread chunk_server_thread;
  chunk_server_thread.start(&test_chunk_server, NULL);
  sleep(2);

  ObGetParam param;
  ObCellInfo cell;
  cell.table_id_ = 1;
  cell.column_id_ = 2;
  param.add_cell(cell);
  cell.table_id_ = 2;
  cell.column_id_ = 1;
  param.add_cell(cell);
  ObScanner scanner;
  EXPECT_TRUE(OB_SUCCESS == stub.get(timeout, chunk_server, param, scanner));
  EXPECT_TRUE(!scanner.is_empty());

  // check result
  uint64_t count = 0;
  ObScannerIterator iter;
  for (iter = scanner.begin(); iter != scanner.end(); ++iter) {
    EXPECT_TRUE(OB_SUCCESS == iter.get_cell(cell));
    //EXPECT_TRUE(cell.column_id_ == count);
    printf("client:%.*s\n", cell.row_key_.length(), cell.row_key_.ptr());
    ++count;
  }
  // return 10 cells
  EXPECT_TRUE(count == 10);

  EXPECT_TRUE(OB_SUCCESS == stub.get(timeout, chunk_server, param, scanner));
  EXPECT_TRUE(!scanner.is_empty());
  for (iter = scanner.begin(); iter != scanner.end(); ++iter) {
    EXPECT_TRUE(OB_SUCCESS == iter.get_cell(cell));
    printf("client:%.*s\n", cell.row_key_.length(), cell.row_key_.ptr());
  }

  transport.stop();
  server.stop();
  sleep(10);
}

TEST_F(TestRpcStub, test_scan_servers) {
  ObMergerRpcStub stub;
  ThreadSpecificBuffer buffer;
  ObPacketFactory factory;
  tbnet::Transport transport;
  tbnet::DefaultPacketStreamer streamer;
  streamer.setPacketFactory(&factory);
  transport.start();
  ObClientManager client_manager;

  EXPECT_TRUE(OB_SUCCESS == client_manager.initialize(&transport, &streamer));
  EXPECT_TRUE(OB_SUCCESS == stub.init(&buffer, &client_manager));

  ObMergerTabletLocationList list;

  ObServer chunk_server;
  chunk_server.set_ipv4_addr(addr, MockChunkServer::CHUNK_SERVER_PORT);

  ObTabletLocation addr;
  //addr.tablet_id_ = 100;
  addr.chunkserver_ = chunk_server;

  list.add(addr);
  list.add(addr);
  list.add(addr);

  // start root server
  MockChunkServer server;
  MockServerRunner test_chunk_server(server);
  tbsys::CThread chunk_server_thread;
  chunk_server_thread.start(&test_chunk_server, NULL);
  sleep(2);

  ObScanParam param;
  ObCellInfo cell;
  ObRange range;
  ObString name;
  ObScanner scanner;

  param.set(1, name, range);
  EXPECT_TRUE(OB_SUCCESS == stub.scan(timeout, chunk_server, param, scanner));
  EXPECT_TRUE(!scanner.is_empty());

  uint64_t count = 0;
  ObScannerIterator iter;
  for (iter = scanner.begin(); iter != scanner.end(); ++iter) {
    EXPECT_TRUE(OB_SUCCESS == iter.get_cell(cell));
    //EXPECT_TRUE(cell.column_id_ == count);
    printf("client:%.*s\n", cell.row_key_.length(), cell.row_key_.ptr());
    ++count;
  }
  // return 10 cells
  EXPECT_TRUE(count == 10);
  ObMergerTabletLocation succ_addr;
  bool update = false;
  EXPECT_TRUE(OB_SUCCESS == stub.scan(timeout, list, param, succ_addr, scanner, update));
  EXPECT_TRUE(!scanner.is_empty());
  EXPECT_TRUE(update == false);
  for (iter = scanner.begin(); iter != scanner.end(); ++iter) {
    EXPECT_TRUE(OB_SUCCESS == iter.get_cell(cell));
    printf("client:%.*s\n", cell.row_key_.length(), cell.row_key_.ptr());
  }

  transport.stop();
  server.stop();
  sleep(10);
}



