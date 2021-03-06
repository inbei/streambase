/**
 * (C) 2010-2011 Alibaba Group Holding Limited.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * Version: $Id$
 *
 * batch_packet_queue_thread.cc for ...
 *
 * Authors:
 *   rizhao <rizhao.ych@taobao.com>
 *
 */
#include "tbnet.h"
#include "batch_packet_queue_thread.h"

namespace sb {
namespace common {

using namespace tbnet;

// 构造
BatchPacketQueueThread::BatchPacketQueueThread() : tbsys::CDefaultRunnable() {
  _stop = 0;
  _waitFinish = false;
  _handler = NULL;
  _args = NULL;
  _waitTime = 0;
  _waiting = false;

  _speed_t2 = _speed_t1 = tbsys::CTimeUtil::getTime();
  _overage = 0;
  _queue.init();
}

// 构造
BatchPacketQueueThread::BatchPacketQueueThread(int threadCount, IBatchPacketQueueHandler* handler, void* args)
  : tbsys::CDefaultRunnable(threadCount) {
  _stop = 0;
  _waitFinish = false;
  _handler = handler;
  _args = args;
  _waitTime = 0;
  _waiting = false;

  _speed_t2 = _speed_t1 = tbsys::CTimeUtil::getTime();
  _overage = 0;
  _queue.init();
}

// 析构
BatchPacketQueueThread::~BatchPacketQueueThread() {
  stop();
}

// 线程参数设置
void BatchPacketQueueThread::setThreadParameter(int threadCount, IBatchPacketQueueHandler* handler, void* args) {
  setThreadCount(threadCount);
  _handler = handler;
  _args = args;
}

// stop
void BatchPacketQueueThread::stop(bool waitFinish) {
  _cond.lock();
  _stop = true;
  _waitFinish = waitFinish;
  _cond.broadcast();
  _cond.unlock();
}

// push
// block==true, this thread can wait util _queue.size less than maxQueueLen
// otherwise, return false directly, client must be free this packet.
bool BatchPacketQueueThread::push(ObPacket* packet, int maxQueueLen, bool block) {
  // 是停止就不允许放了
  if (_stop || _thread == NULL) {
    //delete packet;
    return true;
  }
  // 是否要限制push长度
  if (maxQueueLen > 0 && _queue.size() >= maxQueueLen) {
    _pushcond.lock();
    _waiting = true;
    while (_stop == false && _queue.size() >= maxQueueLen && block) {
      _pushcond.wait(1000);
    }
    _waiting = false;
    if (_queue.size() >= maxQueueLen && !block) {
      _pushcond.unlock();
      return false;
    }
    _pushcond.unlock();

    if (_stop) {
      //delete packet;
      return true;
    }
  }

  // 加锁写入队列
  _cond.lock();
  _queue.push(packet);
  _cond.unlock();
  _cond.signal();
  return true;
}

// pushQueue
void BatchPacketQueueThread::pushQueue(ObPacketQueue& packetQueue, int maxQueueLen) {
  // 是停止就不允许放了
  if (_stop) {
    return;
  }

  // 是否要限制push长度
  if (maxQueueLen > 0 && _queue.size() >= maxQueueLen) {
    _pushcond.lock();
    _waiting = true;
    while (_stop == false && _queue.size() >= maxQueueLen) {
      _pushcond.wait(1000);
    }
    _waiting = false;
    _pushcond.unlock();
    if (_stop) {
      return;
    }
  }

  // 加锁写入队列
  _cond.lock();
  packetQueue.move_to(&_queue);
  _cond.unlock();
  _cond.signal();
}

// Runnable 接口
void BatchPacketQueueThread::run(tbsys::CThread*, void*) {
  int err = OB_SUCCESS;
  tbnet::Packet* tmp_packet = NULL;
  while (!_stop) {
    _cond.lock();
    while (!_stop && _queue.size() == 0) {
      _cond.wait();
    }
    if (_stop) {
      _cond.unlock();
      break;
    }

    // 限速
    if (_waitTime > 0) checkSendSpeed();
    tbnet::Packet* packets[MAX_BATCH_NUM];
    int64_t batch_num = 0;
    // 取出packet
    /*
    while (batch_num < MAX_BATCH_NUM && _queue.size() > 0)
    {
      tmp_packet = _queue.pop();
      // 空的packet?
      if (tmp_packet == NULL) continue;
      packets[batch_num++] = tmp_packet;
    }
    */
    err = _queue.pop_packets(packets, MAX_BATCH_NUM, batch_num);
    if (OB_SUCCESS != err) {
      TBSYS_LOG(ERROR, "failed to pop packets, err=%d", err);
    }
    _cond.unlock();

    // push 在等吗?
    if (_waiting) {
      _pushcond.lock();
      _pushcond.signal();
      _pushcond.unlock();
    }

    bool ret = true;
    if (_handler && batch_num > 0) {
      ret = _handler->handleBatchPacketQueue(batch_num, packets, _args);
    }
    // 如果返回false, 不删除
    // if (ret) {
    //   for (int64_t i = 0; i < batch_num; ++i)
    //   {
    //     delete packets[i];
    //   }
    // }
  }
  if (_waitFinish) { // 把queue中所有的task做完
    bool ret = true;
    _cond.lock();
    while (_queue.size() > 0) {
      tmp_packet = (ObPacket*) _queue.pop();
      _cond.unlock();

      if (_handler) {
        ret = _handler->handleBatchPacketQueue(1, &tmp_packet, _args);
      }
      //if (ret) delete tmp_packet;

      _cond.lock();
    }
    _cond.unlock();
  } else {   // 把queue中的free掉
    _cond.lock();
    while (_queue.size() > 0) {
      _queue.pop();
    }
    _cond.unlock();
  }
}

// 是否计算处理速度
void BatchPacketQueueThread::setStatSpeed() {
}

// 设置限速
void BatchPacketQueueThread::setWaitTime(int t) {
  _waitTime = t;
  _speed_t2 = _speed_t1 = tbsys::CTimeUtil::getTime();
  _overage = 0;
}

// 计算发送速度
void BatchPacketQueueThread::checkSendSpeed() {
  if (_waitTime > _overage) {
    usleep(_waitTime - _overage);
  }
  _speed_t2 = tbsys::CTimeUtil::getTime();
  _overage += (_speed_t2 - _speed_t1) - _waitTime;
  if (_overage > (_waitTime << 4)) _overage = 0;
  _speed_t1 = _speed_t2;
}

void BatchPacketQueueThread::clear() {
  _stop = false;
  delete[] _thread;
  _thread = NULL;
}

}
}


