// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_TIMER_H
#define MUDUO_NET_TIMER_H

#include <boost/noncopyable.hpp>

#include <muduo/base/Atomic.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>

namespace muduo
{
namespace net
{
///
/// Internal class for timer event.
///
class Timer : boost::noncopyable
{
 public:
  // cb是任务处理函数
  // when是何时执行任务
  // interval 定时任务的间隔时间，如果该值为0，那么该任务不需要重复执行，只需要一次即可
  Timer(const TimerCallback& cb, Timestamp when, double interval)
    : callback_(cb),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0), // 根据interval确定是否需要重复
      sequence_(s_numCreated_.incrementAndGet())
  { }

#ifdef __GXX_EXPERIMENTAL_CXX0X__
  Timer(TimerCallback&& cb, Timestamp when, double interval)
    : callback_(std::move(cb)),
      expiration_(when),
      interval_(interval),
      repeat_(interval > 0.0),
      sequence_(s_numCreated_.incrementAndGet())
  { }
#endif

  void run() const
  {
    callback_(); // 指定回调函数
  }

  // 获取任务的本次到期时间
  Timestamp expiration() const  { return expiration_; }
  // 是否需要重复
  bool repeat() const { return repeat_; }
  // 本任务的序列号
  int64_t sequence() const { return sequence_; }

  // 重置任务，主要是针对需要重复执行的任务，详见cc文件
  void restart(Timestamp now);

  static int64_t numCreated() { return s_numCreated_.get(); }

 private:
  const TimerCallback callback_; // 任务处理函数
  Timestamp expiration_;  // 本次过期时间，这个后面要不断的更新
  const double interval_; // 定时任务之间的间隔
  const bool repeat_;     // 是否重复执行
  const int64_t sequence_; // 本任务的序列号

  static AtomicInt64 s_numCreated_; //一个64位的原子数，主要用于计算定时任务的数量
};
}
}
#endif  // MUDUO_NET_TIMER_H
