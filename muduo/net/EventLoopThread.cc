// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoopThread.h>

#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;


EventLoopThread::EventLoopThread(const ThreadInitCallback& cb)
  : loop_(NULL),
    exiting_(false),
    thread_(boost::bind(&EventLoopThread::threadFunc, this), "EventLoopThread"), // FIXME: number it
    mutex_(),
    cond_(mutex_),
    callback_(cb)
{
}

// 此EventLoop的退出流程：
// 1.EventLoopThread对象过期
// 2.如果loop_不为NULL，说明正在执行（因为loop初始为NULL，一旦运行起来，就再也不是NULL）
// 需要将内部的EventLoop关闭
// 3.join线程
EventLoopThread::~EventLoopThread()
{
  exiting_ = true;
  if (loop_ != NULL) // not 100% race-free, eg. threadFunc could be running callback_.
  {
    // still a tiny chance to call destructed object, if threadFunc exits just now.
    // but when EventLoopThread destructs, usually programming is exiting anyway.
    loop_->quit();
    thread_.join();
  }
}

// 开启线程，并且返回持有的EventLoop的指针
EventLoop* EventLoopThread::startLoop()
{
  assert(!thread_.started());
  thread_.start();

  // 这里存在一个race Condition
  // 线程刚刚启动时，里面的EventLoop的指针还没有赋给loop_，所以
  // 这里需要等待，当指针赋值成功后，cond_会通知这里继续执行

  {
    MutexLockGuard lock(mutex_);
    while (loop_ == NULL) // 这里必须使用while循环等待
    {
      cond_.wait();
    }
  }

  return loop_;
}

// 开启的新线程的执行逻辑
void EventLoopThread::threadFunc()
{
  // 因为EventLoopThread运行在一个线程中，而
  // 本函数又代表了线程的全部执行逻辑，所以这里将loop作为
  // 一个栈对象，线程执行结束后，会自动销毁
  EventLoop loop;

  // 执行线程的初始化回调函数，主要与loop有关
  if (callback_)
  {
    callback_(&loop);
  }

  // 这里主要是消除race condition 防止上面的startLoop返回一个空指针
  {
    MutexLockGuard lock(mutex_);
    loop_ = &loop;
    cond_.notify();
  }

  loop.loop();
  //assert(exiting_);
  loop_ = NULL;
}

