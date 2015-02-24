#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 这个版本，相对于server_threaded版本，借助shared_ptr，实现了写时复制，
// 当仅仅读取connections_时，不必加锁，提高了程序的性能

class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3)),
    connections_(new ConnectionList)
  {
    server_.setConnectionCallback(
        boost::bind(&ChatServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&LengthHeaderCodec::onMessage, &codec_, _1, _2, _3));
  }

  void setThreadNum(int numThreads)
  {
    server_.setThreadNum(numThreads);
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

    MutexLockGuard lock(mutex_);
    // 当connections_的引用计数大于1时
    // 说明此时有其他地方在使用这个数据
    if (!connections_.unique()) 
    {
      // 创建一个新的copy
      connections_.reset(new ConnectionList(*connections_));
    }
    assert(connections_.unique());

    if (conn->connected())
    {
      connections_->insert(conn);
    }
    else
    {
      connections_->erase(conn);
    }
  }

  typedef std::set<TcpConnectionPtr> ConnectionList; // 客户连接的集合
  typedef boost::shared_ptr<ConnectionList> ConnectionListPtr; // 客户连接集合的指针

  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 获取一个智能指针
    // 这里不用加锁，因为这里在使用时，connections_的引用计数大于1
    // 所以当在onConnection中试图改动connections_时，会创建一个新的副本
    // 这实际上是一种写时复制技术
    ConnectionListPtr connections = getConnectionList();;
    for (ConnectionList::iterator it = connections->begin();
        it != connections->end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
  }

  // 返回的不是一个副本，而是一个智能指针
  ConnectionListPtr getConnectionList()
  {
    MutexLockGuard lock(mutex_);
    return connections_;
  }

  TcpServer server_;
  LengthHeaderCodec codec_;
  MutexLock mutex_;
  ConnectionListPtr connections_; // 这是set的智能指针
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
    if (argc > 2)
    {
      server.setThreadNum(atoi(argv[2]));
    }
    server.start();
    loop.loop();
  }
  else
  {
    printf("Usage: %s port [thread_num]\n", argv[0]);
  }
}

