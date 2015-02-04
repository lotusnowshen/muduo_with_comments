// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/InetAddress.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Endian.h>
#include <muduo/net/SocketsOps.h>

#include <netdb.h>
#include <strings.h>  // bzero
#include <netinet/in.h>

#include <boost/static_assert.hpp>

// INADDR_ANY use (type)value casting.
#pragma GCC diagnostic ignored "-Wold-style-cast"
// INADDR_ANY表示可以绑定任何一块网卡
static const in_addr_t kInaddrAny = INADDR_ANY;
// INADDR_LOOPBACK表示本地环路测试
static const in_addr_t kInaddrLoopback = INADDR_LOOPBACK;
#pragma GCC diagnostic error "-Wold-style-cast"

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

using namespace muduo;
using namespace muduo::net;

// 使用静态断言，保证addr和InetAddress的size相同，保证兼容性
BOOST_STATIC_ASSERT(sizeof(InetAddress) == sizeof(struct sockaddr_in));

InetAddress::InetAddress(uint16_t port, bool loopbackOnly)
{
  bzero(&addr_, sizeof addr_);
  addr_.sin_family = AF_INET;
  // 根据loopbackOnly决定是采用INADDR_ANY还是INADDR_LOOPBACK
  in_addr_t ip = loopbackOnly ? kInaddrLoopback : kInaddrAny;
  addr_.sin_addr.s_addr = sockets::hostToNetwork32(ip);
  addr_.sin_port = sockets::hostToNetwork16(port);
}

InetAddress::InetAddress(StringArg ip, uint16_t port)
{
  bzero(&addr_, sizeof addr_);
  sockets::fromIpPort(ip.c_str(), port, &addr_);
}

string InetAddress::toIpPort() const
{
  char buf[32];
  sockets::toIpPort(buf, sizeof buf, addr_);
  return buf;
}

string InetAddress::toIp() const
{
  char buf[32];
  sockets::toIp(buf, sizeof buf, addr_);
  return buf;
}

uint16_t InetAddress::toPort() const
{
  return sockets::networkToHost16(addr_.sin_port);
}

// __thread代表线程内全局变量，多个线程访问不会相互干扰，
// 这是gcc提供的功能，它使得下面的resolve变成一个线程安全的函数
static __thread char t_resolveBuffer[64 * 1024];

// DNS解析，输入一个主机名，解析为ip地址，将结果保存在out中
bool InetAddress::resolve(StringArg hostname, InetAddress* out)
{
  assert(out != NULL);
  struct hostent hent;
  struct hostent* he = NULL;
  int herrno = 0;
  bzero(&hent, sizeof(hent));

  // gethostbyname_r是一个可重入的、线程安全的函数，相比之下，gethostbyname使用的是
  // static局部变量存储结果，使用看似简单，实际上丧失了可重入的能力
  // struct hostent *gethostbyname(const char *name);
  // 而gethostbyname_r采用外部传入存储空间，由用户自己保证存储空间的线程安全性即可
  // 当然，如果将本函数用于信号处理函数handler中，导致同一函数对本函数调用两次或以上
  // 仍然会导致结果错误
  int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
  if (ret == 0 && he != NULL)
  {
    assert(he->h_addrtype == AF_INET && he->h_length == sizeof(uint32_t));
    out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
    return true;
  }
  else
  {
    if (ret)
    {
      LOG_SYSERR << "InetAddress::resolve";
    }
    return false;
  }
}
