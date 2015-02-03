// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_POLLER_H
#define MUDUO_NET_POLLER_H

#include <map>
#include <vector>
#include <boost/noncopyable.hpp>

#include <muduo/base/Timestamp.h>
#include <muduo/net/EventLoop.h>

namespace muduo
{
namespace net
{

class Channel;

///
/// Base class for IO Multiplexing
///
/// This class doesn't own the Channel objects.

// Poller类是IO复用类的基类，有两个PollPoller和EpollPoller两个子类
// 内部分别采用poll和epoll实现
// 每个EventLoop内内部都持有一个Poller子类对象。
// 但是无论是EventLoop和Poller均没有拥有Channel对象
// 他们仅仅拥有Channel的指针数组

// 更具体一些，Poller内部存在一个数组，保存所有监听Channel的指针。
// 每次Poller进行poll调用，都会将EventLoop中的activeChannels_进行填充
// 另外，Poller内部持有一个Map，实现从fd到对应Channel的映射
// 这个主要用于底层poll或者epoll返回时，需要根据每个fd的revents填充对应的Channel回调事件
// 并且在activeChannels_中进行填充

class Poller : boost::noncopyable
{
 public:
  typedef std::vector<Channel*> ChannelList;

  Poller(EventLoop* loop);
  virtual ~Poller();

  /// Polls the I/O events.
  /// Must be called in the loop thread.
  virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

  /// Changes the interested I/O events.
  /// Must be called in the loop thread.
  // 更新fd的监听事件
  virtual void updateChannel(Channel* channel) = 0;

  /// Remove the channel, when it destructs.
  /// Must be called in the loop thread.
  // 从epoll中移除fd，停止监听
  virtual void removeChannel(Channel* channel) = 0;

  // 判断该epoll模型是否监听了Channel对应的fd
  virtual bool hasChannel(Channel* channel) const;

  // static方法，用于产生一个Poller子类对象
  static Poller* newDefaultPoller(EventLoop* loop);

  // 断言没有跨线程
  void assertInLoopThread() const
  {
    ownerLoop_->assertInLoopThread();
  }

 protected:
  // 这是极为重要的一个数据结构，记录从fd到Channel的对应关系
  // 底层的epoll每次监听完fd，都要根据这个映射关系去寻找对应的Channel
  typedef std::map<int, Channel*> ChannelMap;
  ChannelMap channels_; // 保存epoll监听的fd，对应的Channel指针

 private:
  EventLoop* ownerLoop_; // 这个Poller对象所属的EventLoop
};

}
}
#endif  // MUDUO_NET_POLLER_H
