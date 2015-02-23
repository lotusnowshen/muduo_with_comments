#include <muduo/base/Logging.h>
#include <muduo/net/Channel.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/Socket.h>
#include <muduo/net/SocketsOps.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

/*

本地测试结果如下：
20150223 09:17:03.888443Z 10384 INFO  round trip 95 clock error 22 - roundtrip_udp.cc:93
20150223 09:17:04.088537Z 10384 INFO  round trip 72 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:04.289798Z 10384 INFO  round trip 72 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:04.490973Z 10384 INFO  round trip 73 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:04.692054Z 10384 INFO  round trip 144 clock error 36 - roundtrip_udp.cc:93
20150223 09:17:04.893189Z 10384 INFO  round trip 73 clock error 11 - roundtrip_udp.cc:93
20150223 09:17:05.093402Z 10384 INFO  round trip 76 clock error 11 - roundtrip_udp.cc:93
20150223 09:17:05.293665Z 10384 INFO  round trip 108 clock error 30 - roundtrip_udp.cc:93
20150223 09:17:05.494841Z 10384 INFO  round trip 114 clock error 21 - roundtrip_udp.cc:93
20150223 09:17:05.694978Z 10384 INFO  round trip 70 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:05.896129Z 10384 INFO  round trip 73 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:06.097480Z 10384 INFO  round trip 73 clock error 11 - roundtrip_udp.cc:93
20150223 09:17:06.298132Z 10384 INFO  round trip 71 clock error 11 - roundtrip_udp.cc:93
20150223 09:17:06.498334Z 10384 INFO  round trip 71 clock error 10 - roundtrip_udp.cc:93
20150223 09:17:06.698734Z 10384 INFO  round trip 170 clock error 16 - roundtrip_udp.cc:93

RTT波动较大，clock err稳定在93上

*/


const size_t frameLen = 2*sizeof(int64_t);

int createNonblockingUDP()
{
  int sockfd = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
  if (sockfd < 0)
  {
    LOG_SYSFATAL << "::socket";
  }
  return sockfd;
}

/////////////////////////////// Server ///////////////////////////////

void serverReadCallback(int sockfd, muduo::Timestamp receiveTime)
{
  int64_t message[2];
  struct sockaddr peerAddr;
  bzero(&peerAddr, sizeof peerAddr);
  socklen_t addrLen = sizeof peerAddr;
  // 接收报文
  ssize_t nr = ::recvfrom(sockfd, message, sizeof message, 0, &peerAddr, &addrLen);

  char addrStr[32];
  sockets::toIpPort(addrStr, sizeof addrStr, *reinterpret_cast<struct sockaddr_in*>(&peerAddr));
  LOG_DEBUG << "received " << nr << " bytes from " << addrStr;

  if (nr < 0)
  {
    LOG_SYSERR << "::recvfrom";
  }
  else if (implicit_cast<size_t>(nr) == frameLen)
  {
    message[1] = receiveTime.microSecondsSinceEpoch();
    // 发送当前时刻
    ssize_t nw = ::sendto(sockfd, message, sizeof message, 0, &peerAddr, addrLen);
    if (nw < 0)
    {
      LOG_SYSERR << "::sendto";
    }
    else if (implicit_cast<size_t>(nw) != frameLen)
    {
      LOG_ERROR << "Expect " << frameLen << " bytes, wrote " << nw << " bytes.";
    }
  }
  else
  {
    LOG_ERROR << "Expect " << frameLen << " bytes, received " << nr << " bytes.";
  }
}

void runServer(uint16_t port)
{
  Socket sock(createNonblockingUDP());
  sock.bindAddress(InetAddress(port));
  EventLoop loop;
  Channel channel(&loop, sock.fd());
  channel.setReadCallback(boost::bind(&serverReadCallback, sock.fd(), _1));
  channel.enableReading();
  loop.loop();
}

/////////////////////////////// Client ///////////////////////////////

void clientReadCallback(int sockfd, muduo::Timestamp receiveTime)
{
  int64_t message[2];
  ssize_t nr = sockets::read(sockfd, message, sizeof message);

  if (nr < 0)
  {
    LOG_SYSERR << "::read";
  }
  else if (implicit_cast<size_t>(nr) == frameLen)
  {
    int64_t send = message[0];
    int64_t their = message[1];
    int64_t back = receiveTime.microSecondsSinceEpoch();
    int64_t mine = (back+send)/2;
    LOG_INFO << "round trip " << back - send
             << " clock error " << their - mine;
  }
  else
  {
    LOG_ERROR << "Expect " << frameLen << " bytes, received " << nr << " bytes.";
  }
}

void sendMyTime(int sockfd)
{
  int64_t message[2] = { 0, 0 };
  message[0] = Timestamp::now().microSecondsSinceEpoch();
  ssize_t nw = sockets::write(sockfd, message, sizeof message);
  if (nw < 0)
  {
    LOG_SYSERR << "::write";
  }
  else if (implicit_cast<size_t>(nw) != frameLen)
  {
    LOG_ERROR << "Expect " << frameLen << " bytes, wrote " << nw << " bytes.";
  }
}

void runClient(const char* ip, uint16_t port)
{
  Socket sock(createNonblockingUDP());
  InetAddress serverAddr(ip, port);
  int ret = sockets::connect(sock.fd(), serverAddr.getSockAddrInet());
  if (ret < 0)
  {
    LOG_SYSFATAL << "::connect";
  }
  EventLoop loop;
  Channel channel(&loop, sock.fd());
  channel.setReadCallback(boost::bind(&clientReadCallback, sock.fd(), _1));
  channel.enableReading();
  loop.runEvery(0.2, boost::bind(sendMyTime, sock.fd()));
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

