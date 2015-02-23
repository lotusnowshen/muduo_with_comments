#include <muduo/net/EventLoop.h>

#include <iostream>
#include <boost/bind.hpp>

// 对全局函数进行bind后注册进定时器
void print(muduo::net::EventLoop* loop, int* count)
{
  if (*count < 5)
  {
    std::cout << *count << "\n";
    ++(*count);

    loop->runAfter(1, boost::bind(print, loop, count));
  }
  else
  {
    loop->quit();
  }
}

int main()
{
  muduo::net::EventLoop loop;
  int count = 0;
  // Note: loop.runEvery() is better for this use case.
  loop.runAfter(1, boost::bind(print, &loop, &count)); // 1s后执行
  loop.loop();
  std::cout << "Final count is " << count << "\n";
}

