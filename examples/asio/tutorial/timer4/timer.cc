#include <muduo/net/EventLoop.h>

#include <iostream>
#include <boost/bind.hpp>
#include <boost/noncopyable.hpp>


// 这个示例，在main中注册也可以

class Printer : boost::noncopyable
{
 public:
  Printer(muduo::net::EventLoop* loop)
    : loop_(loop),
      count_(0)
  {
    // Note: loop.runEvery() is better for this use case.
    // 1s后执行
    loop_->runAfter(1, boost::bind(&Printer::print, this));
  }

  ~Printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print()
  {
    if (count_ < 5)
    {
      std::cout << count_ << "\n";
      ++count_;

      // 1s后执行 当小于5时，每秒执行一次
      loop_->runAfter(1, boost::bind(&Printer::print, this));
    }
    else
    {
      loop_->quit();
    }
  }

private:
  muduo::net::EventLoop* loop_;
  int count_;
};

int main()
{
  muduo::net::EventLoop loop;
  Printer printer(&loop);
  loop.loop();
}

