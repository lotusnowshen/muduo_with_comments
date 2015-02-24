#include "chargen.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

ChargenServer::ChargenServer(EventLoop* loop,
                             const InetAddress& listenAddr,
                             bool print)
  : server_(loop, listenAddr, "ChargenServer"),
    transferred_(0),
    startTime_(Timestamp::now()) // 记录开始时间
{
  server_.setConnectionCallback(
      boost::bind(&ChargenServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&ChargenServer::onMessage, this, _1, _2, _3));
  server_.setWriteCompleteCallback(
      boost::bind(&ChargenServer::onWriteComplete, this, _1));
  if (print)
  {
    // 注册定时器，每3s打印一次传输速率
    loop->runEvery(3.0, boost::bind(&ChargenServer::printThroughput, this));
  }

  // 生成每次要发送的message
  string line;
  for (int i = 33; i < 127; ++i)
  {
    line.push_back(char(i));
  }
  line += line;

  for (size_t i = 0; i < 127-33; ++i)
  {
    message_ += line.substr(i, 72) + '\n';
  }
}

void ChargenServer::start()
{
  server_.start();
}

void ChargenServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "ChargenServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    conn->setTcpNoDelay(true);
    conn->send(message_);
  }
}

void ChargenServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer* buf,
                              Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " discards " << msg.size()
           << " bytes received at " << time.toString();
}

// 这里仅当上次发送的字节完全写入对方缓冲区时，才发送新的msg
// 所以发送的速度受制于对方接收的速度
void ChargenServer::onWriteComplete(const TcpConnectionPtr& conn)
{
  transferred_ += message_.size(); // 记录总共传输的字节数
  conn->send(message_);
}

// 打印传输速度
void ChargenServer::printThroughput()
{
  Timestamp endTime = Timestamp::now(); // 结束时间
  double time = timeDifference(endTime, startTime_); // 求时间差
  printf("%4.3f MiB/s\n", static_cast<double>(transferred_)/time/1024/1024);
  transferred_ = 0; // 字节清空，重新开始计数
  startTime_ = endTime; // 重新计时
}

