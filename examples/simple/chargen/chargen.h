#ifndef MUDUO_EXAMPLES_SIMPLE_CHARGEN_CHARGEN_H
#define MUDUO_EXAMPLES_SIMPLE_CHARGEN_CHARGEN_H

#include <muduo/net/TcpServer.h>

// RFC 864
class ChargenServer
{
 public:
  ChargenServer(muduo::net::EventLoop* loop,
                const muduo::net::InetAddress& listenAddr,
                bool print = false);

  void start();

 private:
  void onConnection(const muduo::net::TcpConnectionPtr& conn);

  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp time);

  void onWriteComplete(const muduo::net::TcpConnectionPtr& conn);
  void printThroughput();

  muduo::net::TcpServer server_;

  muduo::string message_; // 每次要发送的msg
  int64_t transferred_;  // 发送的字节计数
  muduo::Timestamp startTime_; // 每次的开始时间
};

#endif  // MUDUO_EXAMPLES_SIMPLE_CHARGEN_CHARGEN_H
