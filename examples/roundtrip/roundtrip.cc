#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

// 此例可用于计算两台机器的时延，参考http://blog.csdn.net/solstice/article/details/6335082
/*

本地环路测试如下：
20150223 09:15:12.916585Z 10351 INFO  round trip 80 clock error 19 - roundtrip.cc:82
20150223 09:15:13.117041Z 10351 INFO  round trip 76 clock error 16 - roundtrip.cc:82
20150223 09:15:13.318220Z 10351 INFO  round trip 75 clock error 16 - roundtrip.cc:82
20150223 09:15:13.519616Z 10351 INFO  round trip 77 clock error 16 - roundtrip.cc:82
20150223 09:15:13.720432Z 10351 INFO  round trip 95 clock error 17 - roundtrip.cc:82
20150223 09:15:13.921800Z 10351 INFO  round trip 73 clock error 15 - roundtrip.cc:82
20150223 09:15:14.123128Z 10351 INFO  round trip 83 clock error 18 - roundtrip.cc:82
20150223 09:15:14.323512Z 10351 INFO  round trip 77 clock error 17 - roundtrip.cc:82
20150223 09:15:14.524939Z 10351 INFO  round trip 74 clock error 16 - roundtrip.cc:82
20150223 09:15:14.725604Z 10351 INFO  round trip 77 clock error 16 - roundtrip.cc:82
20150223 09:15:14.926722Z 10351 INFO  round trip 76 clock error 15 - roundtrip.cc:82
20150223 09:15:15.126877Z 10351 INFO  round trip 83 clock error 15 - roundtrip.cc:82
20150223 09:15:15.328138Z 10351 INFO  round trip 76 clock error 15 - roundtrip.cc:82
20150223 09:15:15.528649Z 10351 INFO  round trip 75 clock error 16 - roundtrip.cc:82
20150223 09:15:15.730021Z 10351 INFO  round trip 87 clock error 20 - roundtrip.cc:82

可以看出 RTT在15-20左右，而clock error固定在82

*/

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
  loop.runEvery(0.2, sendMyTime); // 每隔0.2s 发送一次请求报文
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

