#include <muduo/net/EventLoop.h>

#include <iostream>

// 直接将void ()类型的全局函数注册进定时器

void print()
{
  std::cout << "Hello, world!\n";
}

int main()
{
  muduo::net::EventLoop loop;
  loop.runAfter(5, print); // 5s 后执行
  loop.loop();
}

