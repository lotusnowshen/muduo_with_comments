// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_BLOCKINGQUEUE_H
#define MUDUO_BASE_BLOCKINGQUEUE_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>
#include <deque>
#include <assert.h>

namespace muduo
{

// 这是一个无边界的阻塞队列，这意味着它永远也不会满。
// 本质上，这个队列封装了生产者消费者问题，在本例中，我们只需要
// 一个Mutex来保证互斥，一个Condition来同步。当生产者放入产品时，
// 我们需要Condition帮助我们唤醒正在等待的消费者。
// 因为队列无边界，所以生产者不会阻塞，也不需要condition去唤醒他
// 所以只需要一个Condition

template<typename T>
class BlockingQueue : boost::noncopyable
{
 public:
  BlockingQueue()
    : mutex_(),
      notEmpty_(mutex_),
      queue_()
  {
  }

  void put(const T& x)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(x);
    notEmpty_.notify(); // wait morphing saves us
    // http://www.domaigne.com/blog/computing/condvars-signal-with-mutex-locked-or-not/
  }

// 为C++11编写的版本，通过右值引用，可以避免x复制的开销
#ifdef __GXX_EXPERIMENTAL_CXX0X__
  void put(T&& x)
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(std::move(x));
    notEmpty_.notify();
  }
  // FIXME: emplace()
#endif

  T take()
  {
    MutexLockGuard lock(mutex_);
    // always use a while-loop, due to spurious wakeup
    // 调用Condition的wait操作时，必须要上锁，而且必须采用while循环判断
    while (queue_.empty())
    {
      notEmpty_.wait();
    }
    assert(!queue_.empty());
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    T front(std::move(queue_.front()));
#else
    T front(queue_.front());
#endif
    queue_.pop_front();
    return front;
  }

  // 上面在判断队列是否为空时，不能使用下面的size函数，
  // 这样同一线程对同一个Mutex加锁两次，会造成死锁
  size_t size() const
  {
    MutexLockGuard lock(mutex_);
    return queue_.size();
  }

 private:
  // 注意这里的Mutex必须定义在Condition前面，这与成员变量的初始化顺序有关
  mutable MutexLock mutex_;
  Condition         notEmpty_; // 当生产者放入产品后，通知等待的消费者
  std::deque<T>     queue_;
};

}

#endif  // MUDUO_BASE_BLOCKINGQUEUE_H
