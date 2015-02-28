#include <examples/ace/ttcp/common.h>
#include <muduo/base/Timestamp.h>

#undef NDEBUG

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <strings.h>

#include <netinet/in.h>
#include <arpa/inet.h>

/*

  ttcp的阻塞版本，实现较简单，封装好send_n 和 read_n即可
*/

static int acceptOrDie(uint16_t port)
{
  int listenfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(listenfd >= 0);

  int yes = 1;
  // 地址复用
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)))
  {
    perror("setsockopt");
    exit(1);
  }

  struct sockaddr_in addr;
  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(listenfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)))
  {
    perror("bind");
    exit(1);
  }

  if (listen(listenfd, 5))
  {
    perror("listen");
    exit(1);
  }

  struct sockaddr_in peer_addr;
  bzero(&peer_addr, sizeof(peer_addr));
  socklen_t addrlen = 0;
  int sockfd = ::accept(listenfd, reinterpret_cast<struct sockaddr*>(&peer_addr), &addrlen);
  if (sockfd < 0)
  {
    perror("accept");
    exit(1);
  }
  ::close(listenfd); // 关闭listenfd 是不打算具备并发能力
  return sockfd;
}

// 写满n个字节
static int write_n(int sockfd, const void* buf, int length)
{
  int written = 0;
  while (written < length)
  {
    ssize_t nw = ::write(sockfd, static_cast<const char*>(buf) + written, length - written);
    if (nw >= 0)
    {
      written += static_cast<int>(nw);
    }
    else if (errno != EINTR)
    {
      perror("write");
      break;
    }
  }
  return written;
}

// 读满n个字节
static int read_n(int sockfd, void* buf, int length)
{
  int nread = 0;
  while (nread < length)
  {
    ssize_t nr = ::read(sockfd, static_cast<char*>(buf) + nread, length - nread);
    if (nr >= 0)
    {
      nread += static_cast<int>(nr);
    }
    else if (errno != EINTR)
    {
      perror("read");
      break;
    }
  }
  return nread;
}

// 客户端发送消息
void transmit(const Options& opt)
{
  struct sockaddr_in addr = resolveOrDie(opt.host.c_str(), opt.port);
  printf("connecting to %s:%d\n", inet_ntoa(addr.sin_addr), opt.port);

  int sockfd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  assert(sockfd >= 0);
  int ret = ::connect(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));
  if (ret)
  {
    perror("connect");
    printf("Unable to connect %s\n", opt.host.c_str());
    ::close(sockfd);
    return;
  }

  printf("connected\n");
  muduo::Timestamp start(muduo::Timestamp::now());
  struct SessionMessage sessionMessage = { 0, 0 };
  sessionMessage.number = htonl(opt.number);
  sessionMessage.length = htonl(opt.length);
  // 发送消息的长度和数目
  if (write_n(sockfd, &sessionMessage, sizeof(sessionMessage)) != sizeof(sessionMessage))
  {
    perror("write SessionMessage");
    exit(1);
  }

  const int total_len = static_cast<int>(sizeof(int32_t) + opt.length);
  PayloadMessage* payload = static_cast<PayloadMessage*>(::malloc(total_len));
  assert(payload);
  payload->length = htonl(opt.length);
  for (int i = 0; i < opt.length; ++i)
  {
    payload->data[i] = "0123456789ABCDEF"[i % 16];
  }

  for (int i = 0; i < opt.number; ++i)
  {
    // 发送报文
    int nw = write_n(sockfd, payload, total_len);
    assert(nw == total_len);

    int ack = 0;
    // 接收ACK回应
    int nr = read_n(sockfd, &ack, sizeof(ack));
    assert(nr == sizeof(ack));
    ack = ntohl(ack);
    assert(ack == opt.length);
  }

  ::free(payload);
  ::close(sockfd);
  // 统计性能 传输了多少字节 速度是多少
  double elapsed = timeDifference(muduo::Timestamp::now(), start);
  double total_mb = 1.0 * opt.length * opt.number / 1024 / 1024;
  printf("%.3f MiB transferred\n%.3f MiB/s\n", total_mb, total_mb / elapsed);
}

// 服务器接收消息
void receive(const Options& opt)
{
  int sockfd = acceptOrDie(opt.port);

  struct SessionMessage sessionMessage = { 0, 0 };
  // 接收消息的长度和数目
  if (read_n(sockfd, &sessionMessage, sizeof(sessionMessage)) != sizeof(sessionMessage))
  {
    perror("read SessionMessage");
    exit(1);
  }

  sessionMessage.number = ntohl(sessionMessage.number);
  sessionMessage.length = ntohl(sessionMessage.length);
  printf("receive number = %d\nreceive length = %d\n",
         sessionMessage.number, sessionMessage.length);
  const int total_len = static_cast<int>(sizeof(int32_t) + sessionMessage.length);
  // 这是一个C技巧，结构体的大小在运行期间确定
  PayloadMessage* payload = static_cast<PayloadMessage*>(::malloc(total_len));
  assert(payload);

  // 依次接收number条报文
  for (int i = 0; i < sessionMessage.number; ++i)
  {
    payload->length = 0;
    // 接收报文长度
    if (read_n(sockfd, &payload->length, sizeof(payload->length)) != sizeof(payload->length))
    {
      perror("read length");
      exit(1);
    }
    payload->length = ntohl(payload->length);
    assert(payload->length == sessionMessage.length);
    // 接收正式报文
    if (read_n(sockfd, payload->data, payload->length) != payload->length)
    {
      perror("read payload data");
      exit(1);
    }
    int32_t ack = htonl(payload->length);
    // 发送ACK回应
    if (write_n(sockfd, &ack, sizeof(ack)) != sizeof(ack))
    {
      perror("write ack");
      exit(1);
    }
  }
  ::free(payload);
  ::close(sockfd);
}

