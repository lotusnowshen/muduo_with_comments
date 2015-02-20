// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Timer.h>

using namespace muduo;
using namespace muduo::net;

AtomicInt64 Timer::s_numCreated_;

void Timer::restart(Timestamp now)
{
  if (repeat_)
  {
  	// 如果需要重复，那就将时间设置为下次过期的时间
    expiration_ = addTime(now, interval_);
  }
  else
  {
  	// 如果不需要重复，那就将过期时间设置为一个不可用的value
    expiration_ = Timestamp::invalid();
  }
}
