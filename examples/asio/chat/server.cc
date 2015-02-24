#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

/*
  这是TCP分包处理的一个示例。采用的方式是：
  在每条消息前，加上消息的长度
  然后，在收取消息时，先收取4个字节的len，然后跟据len的值，收取一条完整的消息

  具体到数据流程上： 数据 -> LengthHeaderCodec的onMessage(收取完整数据)
                      -> ChatServer的onStringMessage(处理一条完整消息)

  发送数据时，必须使用LengthHeaderCodec的send函数
*/

class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "ChatServer"),
    // 这里将onStringMessage注册给LengthHeaderCodec使用
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
  {
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));
    // 将LengthHeaderCodec的onMessage作为message的回调函数
    server_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void start()
  {
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_INFO << conn->localAddress().toIpPort() << " -> "
             << conn->peerAddress().toIpPort() << " is "
             << (conn->connected() ? "UP" : "DOWN");

    if (conn->connected())
    {
      connections_.insert(conn);
    }
    else
    {
      connections_.erase(conn);
    }
  }

  // 这个函数不是messageCallback，他提供给codec使用
  // messageCallback的含义是当收到消息时触发，
  // 而onStringMessage则是收到一条完整信息时触发
  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 依次发给每个客户
    for (ConnectionList::iterator it = connections_.begin();
        it != connections_.end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;
  TcpServer server_;
  LengthHeaderCodec codec_;
  ConnectionList connections_;
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);
    ChatServer server(&loop, serverAddr);
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port\n", argv[0]);
  }
}

