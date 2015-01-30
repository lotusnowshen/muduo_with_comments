// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/EventLoop.h>

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/Channel.h>
#include <muduo/net/Poller.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TimerQueue.h>

#include <boost/bind.hpp>

#include <signal.h>
#include <sys/eventfd.h>

using namespace muduo;
using namespace muduo::net;

namespace
{

// 线程局部变量，实质是线程内部的全局变量
// 这个变量记录本线程持有的EventLoop的指针
// 一个线程最多持有一个EventLoop，所以创建EventLoop时检查该指针即可
__thread EventLoop* t_loopInThisThread = 0;

// poll或epoll调用的超时事件，默认10s
const int kPollTimeMs = 10000;

int createEventfd()
{
  int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (evtfd < 0)
  {
    LOG_SYSERR << "Failed in eventfd";
    abort();
  }
  return evtfd;
}


// 下面的代码是为了屏蔽SIGPIPE信号
// 对于一条已经关闭的tcp连接，第一次发数据收到RST报文，第二次发收到SIGPIPE信号
// 默认是终止进程
#pragma GCC diagnostic ignored "-Wold-style-cast"
class IgnoreSigPipe
{
 public:
  IgnoreSigPipe()
  {
    ::signal(SIGPIPE, SIG_IGN);
    // LOG_TRACE << "Ignore SIGPIPE";
  }
};
#pragma GCC diagnostic error "-Wold-style-cast"

IgnoreSigPipe initObj; // 这里利用C++的全局对象，在main函数调用前，就屏蔽了SIGPIPE
}

// 返回本线程持有的EventLoop指针
EventLoop* EventLoop::getEventLoopOfCurrentThread()
{
  return t_loopInThisThread;
}

EventLoop::EventLoop()
  : looping_(false),
    quit_(false),
    eventHandling_(false),
    callingPendingFunctors_(false),
    iteration_(0),
    threadId_(CurrentThread::tid()),
    poller_(Poller::newDefaultPoller(this)),
    timerQueue_(new TimerQueue(this)),
    wakeupFd_(createEventfd()),
    wakeupChannel_(new Channel(this, wakeupFd_)),
    currentActiveChannel_(NULL)
{
  LOG_DEBUG << "EventLoop created " << this << " in thread " << threadId_;
  // 如果t_loopInThisThread不为空，那么说明本线程已经开启了一个EventLoop
  if (t_loopInThisThread)
  {
    LOG_FATAL << "Another EventLoop " << t_loopInThisThread
              << " exists in this thread " << threadId_;
  }
  else
  {
    t_loopInThisThread = this;
  }
  wakeupChannel_->setReadCallback(
      boost::bind(&EventLoop::handleRead, this));
  // we are always reading the wakeupfd
  wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
  LOG_DEBUG << "EventLoop " << this << " of thread " << threadId_
            << " destructs in thread " << CurrentThread::tid();
  wakeupChannel_->disableAll();
  wakeupChannel_->remove();
  ::close(wakeupFd_);
  t_loopInThisThread = NULL;
}

void EventLoop::loop()
{
  assert(!looping_); // 禁止重复开启loop
  assertInLoopThread(); // 禁止跨线程
  looping_ = true;
  quit_ = false;  // FIXME: what if someone calls quit() before loop() ?
  LOG_TRACE << "EventLoop " << this << " start looping";

  while (!quit_)
  {
    // 每次poll调用，就是一次重新填充activeChannels_的过程
    // 所以这里需要清空
    activeChannels_.clear();
    // 这一步的实质是进行poll或者epoll_wait调用
    // 根据fd的返回事件，填充对应的Channel，以准备后面执行回调处理事件
    pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
    ++iteration_;
    if (Logger::logLevel() <= Logger::TRACE)
    {
      printActiveChannels();
    }
    // TODO sort channel by priority
    // 开始处理回调事件
    eventHandling_ = true;
    for (ChannelList::iterator it = activeChannels_.begin();
        it != activeChannels_.end(); ++it)
    {
      currentActiveChannel_ = *it;
      // 处理该Channel的回调事件
      currentActiveChannel_->handleEvent(pollReturnTime_);
    }
    currentActiveChannel_ = NULL;
    eventHandling_ = false;
    // 执行任务队列中的任务，这些任务可能是线程池内的IO操作因为不能跨线程
    // 所以被转移到Reactor线程
    doPendingFunctors();
  }

  LOG_TRACE << "EventLoop " << this << " stop looping";
  looping_ = false;
}

void EventLoop::quit()
{
  quit_ = true;
  // There is a chance that loop() just executes while(!quit_) and exists,
  // then EventLoop destructs, then we are accessing an invalid object.
  // Can be fixed using mutex_ in both places.
  if (!isInLoopThread())
  {
    wakeup();
  }
}

// 注释见下面的C++11版本
void EventLoop::runInLoop(const Functor& cb)
{
  if (isInLoopThread())
  {
    cb();
  }
  else
  {
    queueInLoop(cb);
  }
}

// 注释见下面的C++11版本
void EventLoop::queueInLoop(const Functor& cb)
{
  {
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(cb);
  }

  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup();
  }
}

TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb)
{
  return timerQueue_->addTimer(cb, time, 0.0);
}

TimerId EventLoop::runAfter(double delay, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, cb);
}

TimerId EventLoop::runEvery(double interval, const TimerCallback& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(cb, time, interval);
}

#ifdef __GXX_EXPERIMENTAL_CXX0X__
// FIXME: remove duplication

// 向EventLoop中添加任务
void EventLoop::runInLoop(Functor&& cb)
{
  // 这里如果是本线程内操作，就直接执行
  if (isInLoopThread())
  {
    cb();
  }
  else // 如果是跨线程操作，就放入队列
  {
    queueInLoop(std::move(cb));
  }
}

// 向任务队列中添加任务
void EventLoop::queueInLoop(Functor&& cb)
{
  { // 在mutex的保护下添加任务
  MutexLockGuard lock(mutex_);
  pendingFunctors_.push_back(std::move(cb));  // emplace_back
  }

  // 如果是跨线程或者EventLoop正在处理之前的IO任务，那么
  // 需要使用wakeup向eventfd写入数据，唤醒epoll
  if (!isInLoopThread() || callingPendingFunctors_)
  {
    wakeup(); // 这里为什么需要唤醒？
  }
}

TimerId EventLoop::runAt(const Timestamp& time, TimerCallback&& cb)
{
  return timerQueue_->addTimer(std::move(cb), time, 0.0);
}

TimerId EventLoop::runAfter(double delay, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), delay));
  return runAt(time, std::move(cb));
}

TimerId EventLoop::runEvery(double interval, TimerCallback&& cb)
{
  Timestamp time(addTime(Timestamp::now(), interval));
  return timerQueue_->addTimer(std::move(cb), time, interval);
}
#endif

void EventLoop::cancel(TimerId timerId)
{
  return timerQueue_->cancel(timerId);
}

// 更新Channel，实质是更新fd的监听事件，所以归根结底
// 要去调用Poller中函数
void EventLoop::updateChannel(Channel* channel)
{
  // 首先必须保证该Channel属于该EventLoop
  assert(channel->ownerLoop() == this);
  // 禁止跨线程
  assertInLoopThread();
  // 从poller中更新fd的监听事件
  poller_->updateChannel(channel);
}

// 移除Channel
void EventLoop::removeChannel(Channel* channel)
{
  // 该Channel必须位于本EventLoop内
  assert(channel->ownerLoop() == this);
  // 禁止跨线程
  assertInLoopThread();
  if (eventHandling_)
  {
    assert(currentActiveChannel_ == channel ||
        std::find(activeChannels_.begin(), activeChannels_.end(), channel) == activeChannels_.end());
  }
  // 从poller中移除该fd，停止对该fd的监听
  poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel)
{
  assert(channel->ownerLoop() == this);
  assertInLoopThread();
  return poller_->hasChannel(channel);
}

// 当跨线程进行一些不允许跨线程的操作时，打印出错误信息，并挂掉程序
void EventLoop::abortNotInLoopThread()
{
  LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
            << " was created in threadId_ = " << threadId_
            << ", current thread id = " <<  CurrentThread::tid();
}

// 每个EventLoop内部持有了一个eventfd包装的Channel
// 用来激活poll调用
void EventLoop::wakeup()
{
  uint64_t one = 1;
  // 写入8个字节，无实际意义
  ssize_t n = sockets::write(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::wakeup() writes " << n << " bytes instead of 8";
  }
}

// 处理eventfd的读事件，也是对应Channel的读事件回调函数
void EventLoop::handleRead()
{
  uint64_t one = 1;
  // 从eventfd中读出8个字节，防止poll被重复触发
  ssize_t n = sockets::read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR << "EventLoop::handleRead() reads " << n << " bytes instead of 8";
  }
}

// 处理任务队列中事件
void EventLoop::doPendingFunctors()
{
  std::vector<Functor> functors;
  callingPendingFunctors_ = true;

  // 这里使用了缩减临界区代码的技巧，减少锁的争用
  // 这里之所以会出现并发的问题，是因为本函数不会跨线程
  // 但是runInLoop可以跨函数，会更改pendingFunctors_
  // 所以这里需要加锁
  {
  MutexLockGuard lock(mutex_);
  functors.swap(pendingFunctors_); // 这里使用swap，取出了任务，同时清空了pendingFunctors_
  }

  // 依次执行任务
  for (size_t i = 0; i < functors.size(); ++i)
  {
    functors[i]();
  }
  // callingPendingFunctors_是个标示，表示EventLoop是否在处理任务
  callingPendingFunctors_ = false;
}

void EventLoop::printActiveChannels() const
{
  for (ChannelList::const_iterator it = activeChannels_.begin();
      it != activeChannels_.end(); ++it)
  {
    const Channel* ch = *it;
    LOG_TRACE << "{" << ch->reventsToString() << "} ";
  }
}

