// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_CHANNEL_H
#define MUDUO_NET_CHANNEL_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

#include <muduo/base/Timestamp.h>

namespace muduo
{
namespace net
{

class EventLoop;

///
/// A selectable I/O channel.
///
/// This class doesn't own the file descriptor.
/// The file descriptor could be a socket,
/// an eventfd, a timerfd, or a signalfd

// Channel对应一个文件描述符fd
// Channel封装了一系列该fd对应的操作，使用回调函数的手法，
// 包括可读、可写、关闭和错误处理四个回调函数
// fd一般是tcp连接，也可以是其他，例如timerfd，甚至是文件fd

// Channel的生命周期并不属于持有它的EventLoop，也不属于Poller，
// 而是属于它的上一层，一般是TcpConnection、Accepor、Connector等，
// 以及自己定义的class（内部持有一个Channel，一般是作为成员变量）

class Channel : boost::noncopyable
{
 public:
  typedef boost::function<void()> EventCallback; // 事件回调函数
  typedef boost::function<void(Timestamp)> ReadEventCallback; // 读操作回调函数，需要传入时间

  Channel(EventLoop* loop, int fd);
  ~Channel();

  // 处理回调事件，一般由poller通过eventLoop来调用
  void handleEvent(Timestamp receiveTime);

  // 设置四种回调函数
  void setReadCallback(const ReadEventCallback& cb)
  { readCallback_ = cb; }
  void setWriteCallback(const EventCallback& cb)
  { writeCallback_ = cb; }
  void setCloseCallback(const EventCallback& cb)
  { closeCallback_ = cb; }
  void setErrorCallback(const EventCallback& cb)
  { errorCallback_ = cb; }

  // 设置回调函数的C++11版本，使用了右值语义
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void setReadCallback(ReadEventCallback&& cb)
  { readCallback_ = std::move(cb); }
  void setWriteCallback(EventCallback&& cb)
  { writeCallback_ = std::move(cb); }
  void setCloseCallback(EventCallback&& cb)
  { closeCallback_ = std::move(cb); }
  void setErrorCallback(EventCallback&& cb)
  { errorCallback_ = std::move(cb); }
#endif

  /// Tie this channel to the owner object managed by shared_ptr,
  /// prevent the owner object being destroyed in handleEvent.
  void tie(const boost::shared_ptr<void>&);

  int fd() const { return fd_; } // 返回该Channel对应的fd
  int events() const { return events_; } // 返回该Channel正在监听的事件
  void set_revents(int revt) { revents_ = revt; } // 进行poll或者epoll_wait调用后，根据fd的返回事件调用此函数
  // int revents() const { return revents_; }
  bool isNoneEvent() const { return events_ == kNoneEvent; } // 判断Channel是否没有事件处理

  // update主要是通过eventloop去更新epoll中fd的监听事件
  void enableReading() { events_ |= kReadEvent; update(); }  // 开始监听读事件
  void disableReading() { events_ &= ~kReadEvent; update(); } // 停止监听读事件
  void enableWriting() { events_ |= kWriteEvent; update(); } // 开始监听写事件 
  void disableWriting() { events_ &= ~kWriteEvent; update(); } // 停止监听写事件
  void disableAll() { events_ = kNoneEvent; update(); } // 停止监听所有事件
  bool isWriting() const { return events_ & kWriteEvent; } // Channel是否在监听写事件，
  // 因为poller只在有数据可写时，才去监听write事件，所以该函数的实际函数是Channel是否在执行写操作

  // 下面函数主要是提供给poller使用
  int index() { return index_; }
  void set_index(int idx) { index_ = idx; }

  // for debug
  string reventsToString() const;

  void doNotLogHup() { logHup_ = false; }

  // 返回持有本Channel的EventLoop指针
  EventLoop* ownerLoop() { return loop_; }
  // 将Channel从EventLoop中移除，其实也要从poller中停止监听该fd
  void remove();

 private:
  void update(); // 通过调用loop中函数，改变本fd在epoll中监听的事件
  void handleEventWithGuard(Timestamp receiveTime); // 在临界区代码中处理事件

  static const int kNoneEvent;
  static const int kReadEvent;
  static const int kWriteEvent;

  EventLoop* loop_; // 持有该Channel的EventLoop的指针
  const int  fd_; // Channel对应的fd
  int        events_; // 该fd正在监听的事件
  int        revents_; // poll调用后，该fd需要处理的事件，依据它，poller调用它相应的回调函数
  int        index_; // used by Poller.
  bool       logHup_;

  boost::weak_ptr<void> tie_;
  bool tied_;
  bool eventHandling_; // 是否正在处理事件
  bool addedToLoop_;

  // 四种回调函数，使用的是boost提供的function模板
  ReadEventCallback readCallback_;
  EventCallback writeCallback_;
  EventCallback closeCallback_;
  EventCallback errorCallback_;
};

}
}
#endif  // MUDUO_NET_CHANNEL_H
