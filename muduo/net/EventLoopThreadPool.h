// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_EVENTLOOPTHREADPOOL_H
#define MUDUO_NET_EVENTLOOPTHREADPOOL_H

#include <vector>
#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

namespace muduo
{

namespace net
{

class EventLoop;
class EventLoopThread;

// 前面的EventLoop和EventLoopThread都是单线程Reactor，区别仅仅是在当前
// 线程内执行loop还是新开一个线程而已。
// 这里的EventLoopThreadPool则是新开一个线程池，池内的每个Thread均执行一个
// EventLoop，另外EventLoopThreadPool从外面接收一个loop作为master
// 它与pool内的EventLoop是一个主从关系

class EventLoopThreadPool : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThreadPool(EventLoop* baseLoop);
  ~EventLoopThreadPool();
  // 设置线程数目 如果不设置，则为0
  void setThreadNum(int numThreads) { numThreads_ = numThreads; }
  void start(const ThreadInitCallback& cb = ThreadInitCallback());

  // 下面是两个调度算法，必须在线程池start后使用
  // valid after calling start()
  /// round-robin
  EventLoop* getNextLoop();

  /// with the same hash code, it will always return the same EventLoop
  EventLoop* getLoopForHash(size_t hashCode);

  std::vector<EventLoop*> getAllLoops(); // 获取线程池所有的EventLoop

  // 判断线程池是否开启
  bool started() const
  { return started_; }

 private:

  EventLoop* baseLoop_; // master Reactor线程 需要构造时从外部接收
  bool started_;  // 线程池是否开启
  int numThreads_; // 线程数目，也就是EventLoop的数目
  int next_; // 用于轮转法中调度
  boost::ptr_vector<EventLoopThread> threads_; //线程指针数组
  std::vector<EventLoop*> loops_;  // 线程池内EventLoop的指针集合
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREADPOOL_H
