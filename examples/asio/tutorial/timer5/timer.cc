#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <iostream>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>

// 这个示例，开启两个loop，进行竞争
// 普通情况下，每个loop竞争到5次机会
class Printer : boost::noncopyable
{
 public:
  Printer(muduo::net::EventLoop* loop1, muduo::net::EventLoop* loop2)
    : loop1_(loop1),
      loop2_(loop2),
      count_(0)
  {
    loop1_->runAfter(1, boost::bind(&Printer::print1, this));
    loop2_->runAfter(1, boost::bind(&Printer::print2, this));
  }

  ~Printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print1()
  {
    muduo::MutexLockGuard lock(mutex_);
    if (count_ < 10)
    {
      std::cout << "Timer 1: " << count_ << "\n";
      ++count_;

      loop1_->runAfter(1, boost::bind(&Printer::print1, this));
    }
    else
    {
      loop1_->quit();
    }
  }

  void print2()
  {
    muduo::MutexLockGuard lock(mutex_);
    if (count_ < 10)
    {
      std::cout << "Timer 2: " << count_ << "\n";
      ++count_;

      loop2_->runAfter(1, boost::bind(&Printer::print2, this));
    }
    else
    {
      loop2_->quit();
    }
  }

private:

  muduo::MutexLock mutex_;
  muduo::net::EventLoop* loop1_;
  muduo::net::EventLoop* loop2_;
  int count_;
};

int main()
{
  boost::scoped_ptr<Printer> printer;  // make sure printer lives longer than loops, to avoid
                                       // race condition of calling print2() on destructed object.
  // 这里使用智能指针，是为了确保printer的生存周期比loop长
  // 如果直接用栈对象，有一个潜在的竞争问题：https://github.com/chenshuo/muduo/pull/80
  // 原来的实现中，printer是最后一个栈对象，执行到最后一行时他也是第一个被销毁的，
  // 但是loop2线程还在等待关闭时
  // loop2可能继续执行其中的定时任务，但是printer等对象全部销毁了，所以造成错误
  // 解决方案很简单，就是将printer的定义提前，但是printer又依赖于两个loop，所以
  // 这里使用scoped_ptr，就可以解决这个问题
  muduo::net::EventLoop loop;
  muduo::net::EventLoopThread loopThread;
  muduo::net::EventLoop* loopInAnotherThread = loopThread.startLoop();
  printer.reset(new Printer(&loop, loopInAnotherThread));
  loop.loop();
}

