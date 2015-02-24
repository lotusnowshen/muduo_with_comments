#include "codec.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Mutex.h>
#include <muduo/base/ThreadLocalSingleton.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>

#include <set>
#include <stdio.h>

using namespace muduo;
using namespace muduo::net;


/*
  这是Chat的一个高性能版本，它的高性能体现在：
  1. 采用了多个Reactor线程
  2. 以往是将connections作为全局变量，这里采用threadlocal存储，每个loop存储自己拥有的连接
      收到消息后，依次命令所有loop往自己的连接发送消息即可
  3. 因为采用了多个Reactor，所以仅仅在分发消息的时候，对loops上锁，里面没有阻塞操作，甚至不涉及IO操作
      如果还需提高性能，可以考虑每次都拷贝一份loops的副本
*/

class ChatServer : boost::noncopyable
{
 public:
  ChatServer(EventLoop* loop,
             const InetAddress& listenAddr)
  : server_(loop, listenAddr, "ChatServer"),
    codec_(boost::bind(&ChatServer::onStringMessage, this, _1, _2, _3))
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
    // 设置loop线程初始化函数
    server_.setThreadInitCallback(boost::bind(&ChatServer::threadInit, this, _1));
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
      LocalConnections::instance().insert(conn);
    }
    else
    {
      LocalConnections::instance().erase(conn);
    }
  }

  void onStringMessage(const TcpConnectionPtr&,
                       const string& message,
                       Timestamp)
  {
    // 分发消息的任务
    EventLoop::Functor f = boost::bind(&ChatServer::distributeMessage, this, message);
    LOG_DEBUG;

    MutexLockGuard lock(mutex_);
    // 依次命令每个loop去执行分发消息的任务
    for (std::set<EventLoop*>::iterator it = loops_.begin();
        it != loops_.end();
        ++it)
    {
      (*it)->queueInLoop(f);
    }
    LOG_DEBUG;
  }

  typedef std::set<TcpConnectionPtr> ConnectionList;

  // 分发消息，这个函数由每个loop去执行
  void distributeMessage(const string& message)
  {
    LOG_DEBUG << "begin";
    // 因为LocalConnections是线程局部的，所以每个loop调用LocalConnections::instance()
    // 获得的connections也是不同的
    for (ConnectionList::iterator it = LocalConnections::instance().begin();
        it != LocalConnections::instance().end();
        ++it)
    {
      codec_.send(get_pointer(*it), message);
    }
    LOG_DEBUG << "end";
  }

  void threadInit(EventLoop* loop)
  {
    // 每个loop线程在初始化时，都先实例化一个ConnectionList，这个是线程局部的
    // 然后将loop加入loops_中
    assert(LocalConnections::pointer() == NULL);
    // 底层调用了pthread_setspecific，设置了一个线程私有变量
    LocalConnections::instance();
    assert(LocalConnections::pointer() != NULL);
    MutexLockGuard lock(mutex_);
    loops_.insert(loop);
  }

  TcpServer server_;
  LengthHeaderCodec codec_;
  // 线程局部的ConnectionList，而且是单例
  typedef ThreadLocalSingleton<ConnectionList> LocalConnections;

  MutexLock mutex_;
  std::set<EventLoop*> loops_; // 线程池中loop的集合
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


