// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>

#include <sstream>

#include <poll.h>

using namespace muduo;
using namespace muduo::net;

// Poll与EPoll中的事件相同，
// POLLIN与EPOLLIN POLLPRI与EPOLLPRI POLLOUT与EPOLLOUT等价
// 这里可以加上静态断言来确定
const int Channel::kNoneEvent = 0; // 没有需要监听的事件
const int Channel::kReadEvent = POLLIN | POLLPRI; // 读事件
const int Channel::kWriteEvent = POLLOUT; // 写事件

Channel::Channel(EventLoop* loop, int fd__)
  : loop_(loop),
    fd_(fd__),
    events_(0),
    revents_(0),
    index_(-1),
    logHup_(true),
    tied_(false),
    eventHandling_(false),
    addedToLoop_(false)
{
}

// 析构时，必须确保loop中已经移除了该fd，或者epoll中已经删除了该fd
Channel::~Channel()
{
  assert(!eventHandling_);
  assert(!addedToLoop_);
  if (loop_->isInLoopThread())
  {
    assert(!loop_->hasChannel(this));
  }
}

void Channel::tie(const boost::shared_ptr<void>& obj)
{
  tie_ = obj;
  tied_ = true;
}

// 通过调用loop中函数，改变本fd在epoll中监听的事件
// 在loop中又会去调用Poller中的函数，来更改epoll对fd监听的事件
void Channel::update()
{
  addedToLoop_ = true;
  loop_->updateChannel(this);
}

// 同上，通过EventLoop调用Epoller中函数，达到从epoll或者poll中删除fd的目的
// 进行该操作时，必须确保该Channel已经不再监听任何事件
void Channel::remove()
{
  assert(isNoneEvent());
  addedToLoop_ = false;
  loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
  boost::shared_ptr<void> guard;
  if (tied_)
  {
    guard = tie_.lock();
    if (guard)
    {
      handleEventWithGuard(receiveTime);
    }
  }
  else
  {
    handleEventWithGuard(receiveTime);
  }
}

// 处理各种回调事件
void Channel::handleEventWithGuard(Timestamp receiveTime)
{
  eventHandling_ = true;
  LOG_TRACE << reventsToString();
  if ((revents_ & POLLHUP) && !(revents_ & POLLIN))
  {
    if (logHup_)
    {
      LOG_WARN << "Channel::handle_event() POLLHUP";
    }
    if (closeCallback_) closeCallback_();
  }

  if (revents_ & POLLNVAL) // fd值非法，可能是fd已经关闭
  {
    LOG_WARN << "Channel::handle_event() POLLNVAL";
  }

  if (revents_ & (POLLERR | POLLNVAL)) // 错误事件处理
  {
    if (errorCallback_) errorCallback_();
  }
  if (revents_ & (POLLIN | POLLPRI | POLLRDHUP)) // 有数据可读
  {
    if (readCallback_) readCallback_(receiveTime);
  }
  if (revents_ & POLLOUT) // 该fd可写
  {
    if (writeCallback_) writeCallback_();
  }
  eventHandling_ = false;
}

string Channel::reventsToString() const
{
  std::ostringstream oss;
  oss << fd_ << ": ";
  if (revents_ & POLLIN)
    oss << "IN ";
  if (revents_ & POLLPRI)
    oss << "PRI ";
  if (revents_ & POLLOUT)
    oss << "OUT ";
  if (revents_ & POLLHUP)
    oss << "HUP ";
  if (revents_ & POLLRDHUP)
    oss << "RDHUP ";
  if (revents_ & POLLERR)
    oss << "ERR ";
  if (revents_ & POLLNVAL)
    oss << "NVAL ";

  return oss.str().c_str();
}
