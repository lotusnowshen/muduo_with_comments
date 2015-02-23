#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 此例可用于计算两台机器的时延，参考http://blog.csdn.net/solstice/article/details/6335082

const size_t frameLen = 2*sizeof(int64_t);

void serverConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
        << conn->localAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    conn->setTcpNoDelay(true); // 禁用nagle算法
  }
  else
  {
  }
}

// message回调函数，演示了muduo处理分包的做法
void serverMessageCallback(const TcpConnectionPtr& conn,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  // 这里必须要使用while循环
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
    // 将server上的事件填充在第二个位置上
    // 这里为什么没有进行字节序的转化？
    message[1] = receiveTime.microSecondsSinceEpoch();
    conn->send(message, sizeof message);
  }
}

void runServer(uint16_t port)
{
  EventLoop loop;
  TcpServer server(&loop, InetAddress(port), "ClockServer");
  server.setConnectionCallback(serverConnectionCallback);
  server.setMessageCallback(serverMessageCallback);
  server.start();
  loop.loop();
}

TcpConnectionPtr clientConnection;

void clientConnectionCallback(const TcpConnectionPtr& conn)
{
  LOG_TRACE << conn->localAddress().toIpPort() << " -> "
        << conn->peerAddress().toIpPort() << " is "
        << (conn->connected() ? "UP" : "DOWN");
  if (conn->connected())
  {
    clientConnection = conn; // 保存客户端连接
    conn->setTcpNoDelay(true);
  }
  else
  {
    clientConnection.reset();
  }
}

void clientMessageCallback(const TcpConnectionPtr&,
                           Buffer* buffer,
                           muduo::Timestamp receiveTime)
{
  int64_t message[2];
  while (buffer->readableBytes() >= frameLen)
  {
    memcpy(message, buffer->peek(), frameLen);
    buffer->retrieve(frameLen);
    int64_t send = message[0]; // 发送时的时间 T1
    int64_t their = message[1]; // 服务器上的事件 T2
    int64_t back = receiveTime.microSecondsSinceEpoch(); // 客户端接收时间 T3
    int64_t mine = (back+send)/2;
    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
  }
}

void sendMyTime()
{
  if (clientConnection)
  {
    int64_t message[2] = { 0, 0 };
    message[0] = Timestamp::now().microSecondsSinceEpoch();
    clientConnection->send(message, sizeof message);
  }
}

// 运行客户端
void runClient(const char* ip, uint16_t port)
{
  EventLoop loop;
  TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
  client.enableRetry();
  client.setConnectionCallback(clientConnectionCallback);
  client.setMessageCallback(clientMessageCallback);
  client.connect();
  loop.runEvery(0.2, sendMyTime); // 每隔2s 发送一次请求报文
  loop.loop();
}

int main(int argc, char* argv[])
{
  if (argc > 2)
  {
    uint16_t port = static_cast<uint16_t>(atoi(argv[2]));
    if (strcmp(argv[1], "-s") == 0)
    {
      runServer(port);
    }
    else
    {
      runClient(argv[1], port);
    }
  }
  else
  {
    printf("Usage:\n%s -s port\n%s ip port\n", argv[0], argv[0]);
  }
}

