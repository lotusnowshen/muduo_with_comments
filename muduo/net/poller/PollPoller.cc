// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/poller/PollPoller.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Types.h>
#include <muduo/net/Channel.h>

#include <assert.h>
#include <errno.h>
#include <poll.h>

using namespace muduo;
using namespace muduo::net;

PollPoller::PollPoller(EventLoop* loop)
  : Poller(loop)
{
}

PollPoller::~PollPoller()
{
}

// 这是EventLoop最核心的函数调用，主要是进行底层的poll调用，然后填充活跃fd的revents，以及Channel
// timeoutMs poll调用的超时时间
// activeChannels是EventLoop中的成员变量，用于保存活跃的fd对应的Channel
Timestamp PollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
  // XXX pollfds_ shouldn't change
  int numEvents = ::poll(&*pollfds_.begin(), pollfds_.size(), timeoutMs);
  int savedErrno = errno;
  Timestamp now(Timestamp::now());
  if (numEvents > 0)
  {
    LOG_TRACE << numEvents << " events happended";
    // 根据活跃fd的revents，填充对应的Channel，以及需要处理的事件
    fillActiveChannels(numEvents, activeChannels);
  }
  else if (numEvents == 0)
  {
    LOG_TRACE << " nothing happended";
  }
  else
  {
    if (savedErrno != EINTR)
    {
      errno = savedErrno;
      LOG_SYSERR << "PollPoller::poll()";
    }
  }
  return now; // 返回值是poll调用返回的时刻，也就是后面每个回调函数执行的时刻
}

// 根据poll调用返回的活跃fd，填充对应的Channel，以及对应的回调函数
// numEvents 指的是活跃的fd数量，也就是Channel的数量
// activeChannels loop的变量，保存活跃的Channel指针
void PollPoller::fillActiveChannels(int numEvents,
                                    ChannelList* activeChannels) const
{
  for (PollFdList::const_iterator pfd = pollfds_.begin();
      pfd != pollfds_.end() && numEvents > 0; ++pfd)
  {
    if (pfd->revents > 0) // 该fd是活跃的
    {
      --numEvents; // 活跃fd数量减一
      // channels_保存了从fd到Channel的对应关系，这里用于查找对应的Channel指针
      ChannelMap::const_iterator ch = channels_.find(pfd->fd);
      assert(ch != channels_.end());
      Channel* channel = ch->second;
      assert(channel->fd() == pfd->fd);
      channel->set_revents(pfd->revents); // 设置fd的revents
      // pfd->revents = 0;
      activeChannels->push_back(channel); // 将Channel的指针放入activeChannels
    }
  }
}

// 更新Channel的监听事件
// channnel  需要处理的Channel指针
void PollPoller::updateChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd() << " events = " << channel->events();
  if (channel->index() < 0) // 这个Channel还没有放入loop，或者说该fd还没有加入poll数组
  {
    // 一个新的fd加入loop的过程
    // 1. 在poll中注册fd
    // 2. 更新Channel中的下标信息
    // 3. 存放从fd到Channel的映射
    // 此时才真正将该Channel纳入loop

    // a new one, add to pollfds_
    assert(channels_.find(channel->fd()) == channels_.end());
    // 下面几行将fd放入poll数组
    struct pollfd pfd;
    pfd.fd = channel->fd();
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    pollfds_.push_back(pfd);
    // 该fd在poll数组的下标存入Channel
    int idx = static_cast<int>(pollfds_.size())-1;
    channel->set_index(idx);
    // 在map映射中保存channel
    channels_[pfd.fd] = channel;
  }
  else
  {
    // update existing one
    assert(channels_.find(channel->fd()) != channels_.end());
    assert(channels_[channel->fd()] == channel);
    // 查找该fd的下标
    int idx = channel->index();
    assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
    struct pollfd& pfd = pollfds_[idx]; // 注意这里采用的是reference！！
    assert(pfd.fd == channel->fd() || pfd.fd == -channel->fd()-1);
    pfd.events = static_cast<short>(channel->events());
    pfd.revents = 0;
    // 该fd不再被监听，应该将其置为fd的相反数减一
    // 为什么要减一？
    // 一般情况下，取消监听fd，是设置为-1
    // 这里这样做，一是考虑到将来可能恢复此fd
    // 二是 0的相反数仍是0，无法判断是否取消监听，所以再次减一
    // 此时只要fd变为负数，那就肯定没有监听
    // 将来如果恢复此fd的监听，那么不需要重新加入，注意fd被取消监听后，Channel中的index仍保留原值
    // 此时只需加1，然后去相反数即可，或者取相反数，然后减一（为什么？）
    if (channel->isNoneEvent())
    {
      // ignore this pollfd
      pfd.fd = -channel->fd()-1;
    }
  }
}

// 移除某Channel
// 这里的步骤是
// 1. 从poll数组中删除
// 2. 从map映射中删除
void PollPoller::removeChannel(Channel* channel)
{
  Poller::assertInLoopThread();
  LOG_TRACE << "fd = " << channel->fd();
  assert(channels_.find(channel->fd()) != channels_.end());
  assert(channels_[channel->fd()] == channel);
  assert(channel->isNoneEvent()); // 该Channel已经取消了监听，所以poll中fd已经变为负数
  int idx = channel->index(); // 取出下标
  assert(0 <= idx && idx < static_cast<int>(pollfds_.size()));
  const struct pollfd& pfd = pollfds_[idx]; (void)pfd;
  assert(pfd.fd == -channel->fd()-1 && pfd.events == channel->events());
  size_t n = channels_.erase(channel->fd()); // 从映射关系中移除
  assert(n == 1); (void)n;
  if (implicit_cast<size_t>(idx) == pollfds_.size()-1) //该fd是fds中最后一个元素
  {
    pollfds_.pop_back(); // 从poll数组中删除
  }
  else
  {
    // 这里经过处理，将fd移至fds的最后一个位置，然后使用pop_back弹出即可，避免了数组的大规模移动

    int channelAtEnd = pollfds_.back().fd; // 取出fds最后一个位置的fd
    // iter_swap 交换两个迭代器所指向的元素
    // 这里交换要移除的fd和最后一个fd
    iter_swap(pollfds_.begin()+idx, pollfds_.end()-1);
    if (channelAtEnd < 0)
    {
      channelAtEnd = -channelAtEnd-1; // 该fd为负数，需要还原成正数
    }
    channels_[channelAtEnd]->set_index(idx); // 更新fds原来的末尾元素的index
    pollfds_.pop_back(); // 移除最后一个fd
  }
}

