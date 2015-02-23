#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThread.h>

#include <stdio.h>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>

//
// Minimize locking
//

// 这个示例相对于上面那个，缩减了临界区代码的长度
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
    // cout is not thread safe
    //std::cout << "Final count is " << count_ << "\n";
    printf("Final count is %d\n", count_);
  }

  void print1()
  {
    bool shouldQuit = false;
    int count = 0;

    // 这里缩减了临界区，仅仅对获取count值得部分，进行了加锁保护
    {
      muduo::MutexLockGuard lock(mutex_);
      if (count_ < 10)
      {
        count = count_;
        ++count_;
      }
      else
      {
        shouldQuit = true;
      }
    }

    // out of lock
    if (shouldQuit)
    {
      // printf("loop1_->quit()\n");
      loop1_->quit();
    }
    else
    {
      // cout is not thread safe
      //std::cout << "Timer 1: " << count << "\n";
      printf("Timer 1: %d\n", count);
      loop1_->runAfter(1, boost::bind(&Printer::print1, this));
    }
  }

  void print2()
  {
    bool shouldQuit = false;
    int count = 0;

    {
      muduo::MutexLockGuard lock(mutex_);
      if (count_ < 10)
      {
        count = count_;
        ++count_;
      }
      else
      {
        shouldQuit = true;
      }
    }

    // out of lock
    if (shouldQuit)
    {
      // printf("loop2_->quit()\n");
      loop2_->quit();
    }
    else
    {
      // cout is not thread safe
      //std::cout << "Timer 2: " << count << "\n";
      printf("Timer 2: %d\n", count);
      loop2_->runAfter(1, boost::bind(&Printer::print2, this));
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
  // 参考time5中得说明
  boost::scoped_ptr<Printer> printer;  // make sure printer lives longer than loops, to avoid
                                       // race condition of calling print2() on destructed object.
  muduo::net::EventLoop loop;
  muduo::net::EventLoopThread loopThread;
  muduo::net::EventLoop* loopInAnotherThread = loopThread.startLoop();
  printer.reset(new Printer(&loop, loopInAnotherThread));
  loop.loop();
}

