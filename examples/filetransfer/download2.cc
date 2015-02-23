#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 这是文件传输的第二个实例，
// 这个实例中，将文件指针fp，保存在tcp连接的上下文中，这样
// 就不必一次性读取所有的文件内容，而是每次读取一部分
// 每次发送字节的时机是onWriteComplete，当上次的字节写入对方缓冲区时，再次发送
// 这里的缺点是，需要手工关闭fp，造成编码困难，容易遗忘
// 在发送完毕和连接关闭两种情况下都需要手工执行fclose

void onHighWaterMark(const TcpConnectionPtr& conn, size_t len)
{
  LOG_INFO << "HighWaterMark " << len;
}

const int kBufSize = 64*1024;
const char* g_file = NULL;

void onConnection(const TcpConnectionPtr& conn)
{
  LOG_INFO << "FileServer - " << conn->peerAddress().toIpPort() << " -> "
           << conn->localAddress().toIpPort() << " is "
           << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    LOG_INFO << "FileServer - Sending file " << g_file
             << " to " << conn->peerAddress().toIpPort();
    conn->setHighWaterMarkCallback(onHighWaterMark, kBufSize+1);

    FILE* fp = ::fopen(g_file, "rb");
    if (fp)
    {
      conn->setContext(fp); // 将fp保存在连接上下文中
      char buf[kBufSize];
      size_t nread = ::fread(buf, 1, sizeof buf, fp);
      conn->send(buf, static_cast<int>(nread));
    }
    else
    {
      conn->shutdown();
      LOG_INFO << "FileServer - no such file";
    }
  }
  else
  {
    if (!conn->getContext().empty())
    {
      FILE* fp = boost::any_cast<FILE*>(conn->getContext());
      if (fp)
      {
        ::fclose(fp);
      }
    }
  }
}

void onWriteComplete(const TcpConnectionPtr& conn)
{
  // 从上下文中提取出文件指针
  FILE* fp = boost::any_cast<FILE*>(conn->getContext());
  char buf[kBufSize];
  size_t nread = ::fread(buf, 1, sizeof buf, fp);
  if (nread > 0)
  {
    conn->send(buf, static_cast<int>(nread));
  }
  else
  {
    ::fclose(fp);
    fp = NULL;
    conn->setContext(fp); // 清空上下文
    conn->shutdown();
    LOG_INFO << "FileServer - done";
  }
}

int main(int argc, char* argv[])
{
  LOG_INFO << "pid = " << getpid();
  if (argc > 1)
  {
    g_file = argv[1];

    EventLoop loop;
    InetAddress listenAddr(2021);
    TcpServer server(&loop, listenAddr, "FileServer");
    server.setConnectionCallback(onConnection);
    server.setWriteCompleteCallback(onWriteComplete);
    server.start();
    loop.loop();
  }
  else
  {
    fprintf(stderr, "Usage: %s file_for_downloading\n", argv[0]);
  }
}

