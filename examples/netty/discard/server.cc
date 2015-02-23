#include <muduo/net/TcpServer.h>

#include <muduo/base/Atomic.h>
#include <muduo/base/Logging.h>
#include <muduo/base/Thread.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <boost/bind.hpp>

#include <utility>

#include <stdio.h>
#include <unistd.h>

using namespace muduo;
using namespace muduo::net;

int numThreads = 0;

class DiscardServer
{
 public:
  DiscardServer(EventLoop* loop, const InetAddress& listenAddr)
    : server_(loop, listenAddr, "DiscardServer"),
      oldCounter_(0),
      startTime_(Timestamp::now())
  {
    server_.setConnectionCallback(
        boost::bind(&DiscardServer::onConnection, this, _1));
    server_.setMessageCallback(
        boost::bind(&DiscardServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
    // 每3s打印一次传输速度
    loop->runEvery(3.0, boost::bind(&DiscardServer::printThroughput, this));
  }

  void start()
  {
    LOG_INFO << "starting " << numThreads << " threads.";
    server_.start();
  }

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    LOG_TRACE << conn->peerAddress().toIpPort() << " -> "
              << conn->localAddress().toIpPort() << " is "
              << (conn->connected() ? "UP" : "DOWN");
  }

  void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
  {
    // 可读取的字节数
    size_t len = buf->readableBytes();
    transferred_.add(len); // 接收字节数目
    receivedMessages_.incrementAndGet(); // 接收消息数目
    buf->retrieveAll(); // 丢弃消息
  }

  void printThroughput()
  {
    Timestamp endTime = Timestamp::now();
    int64_t newCounter = transferred_.get(); // 获取当前的字节数
    int64_t bytes = newCounter - oldCounter_; // 减去旧的字节数，就是这段时间内传输的字节数
    int64_t msgs = receivedMessages_.getAndSet(0); // 这段时间内传输的消息数目，注意后面设置为0
    double time = timeDifference(endTime, startTime_); // 时间差
    printf("%4.3f MiB/s %4.3f Ki Msgs/s %6.2f bytes per msg\n",
        static_cast<double>(bytes)/time/1024/1024,
        static_cast<double>(msgs)/time/1024,
        static_cast<double>(bytes)/static_cast<double>(msgs)); // 平均每条消息的长度

    oldCounter_ = newCounter; // 保存当前的字节数目
    startTime_ = endTime; // 重新计时
  }

  TcpServer server_;

  // 这里采用了多线程，所以这里必须采用原子数，而不是int
  AtomicInt64 transferred_; // 传输的字节数
  AtomicInt64 receivedMessages_; // 接收的消息数目
  int64_t oldCounter_; // 旧的计数器，记录字节数
  Timestamp startTime_; // 开始计数时间
};

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid() << ", tid = " << CurrentThread::tid();
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(2009);
  DiscardServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

