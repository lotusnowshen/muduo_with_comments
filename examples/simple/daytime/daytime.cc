#include "daytime.h"

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>

#include <boost/bind.hpp>

using namespace muduo;
using namespace muduo::net;

DaytimeServer::DaytimeServer(EventLoop* loop,
                             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "DaytimeServer")
{
  server_.setConnectionCallback(
      boost::bind(&DaytimeServer::onConnection, this, _1));
  server_.setMessageCallback(
      boost::bind(&DaytimeServer::onMessage, this, _1, _2, _3));
}

void DaytimeServer::start()
{
  server_.start();
}

void DaytimeServer::onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "DaytimeServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    // 将当前时间，以字符串的方式发送回去，然后关闭连接
    conn->send(Timestamp::now().toFormattedString() + "\n");
    conn->shutdown();
  }
}

// 注意，如果客户端发送过快，在调用shutdown之前，这里很可能接收到消息
void DaytimeServer::onMessage(const TcpConnectionPtr& conn,
                              Buffer* buf,
                              Timestamp time)
{
  string msg(buf->retrieveAllAsString());
  LOG_INFO << conn->name() << " discards " << msg.size()
           << " bytes received at " << time.toString();
}

