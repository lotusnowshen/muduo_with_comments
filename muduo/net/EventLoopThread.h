// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_EVENTLOOPTHREAD_H
#define MUDUO_NET_EVENTLOOPTHREAD_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/Thread.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
namespace net
{

// 之前的EventLoop都是占据了一个线程，但是有些情况下，我们需要使用
// 一个单独的线程去执行EventLoop，例如TCP的client，我们可能需要在
// 当前线程中读取stdin，所以希望EventLoop在一个新的线程中运行
// 这就是EventLoopThread产生的必要性

class EventLoop;

class EventLoopThread : boost::noncopyable
{
 public:
  typedef boost::function<void(EventLoop*)> ThreadInitCallback;

  EventLoopThread(const ThreadInitCallback& cb = ThreadInitCallback());
  ~EventLoopThread();
  EventLoop* startLoop();

 private:
  void threadFunc(); // 线程的回调函数

  // 这个类存在一个race condition，主要是loop初始化一个NULL
  // 当开启loop后才将指针赋给它，所以直接执行startLoop可能
  // 会在Loop开启后但还没有赋值给loop_之前返回，结果外界得到一个空指针，
  // 所以这里需要一个Mutex，实现互斥，一个Condition用于实现线程内给loop_
  // 赋值后，通知startLoop可以返回（因为startLoop可能在等待），以此实现startLoop
  // 返回结果的正确性


  EventLoop* loop_; // 本对象拥有的EventLoop的指针
  bool exiting_; // 
  Thread thread_; // 线程，在内部执行EventLoop
  MutexLock mutex_; // 保护对loop_的互斥操作
  Condition cond_; // 通知startLoop可以返回
  ThreadInitCallback callback_; // 线程初始时的回调函数，只执行一次
};

}
}

#endif  // MUDUO_NET_EVENTLOOPTHREAD_H

