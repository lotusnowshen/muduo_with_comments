// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/Acceptor.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <errno.h>
#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace muduo;
using namespace muduo::net;

// Acceptor这类对象，内部持有一个Channel，和TcpConnection相同，必须在构造函数中设置各种回调函数
// 然后在其他动作中开始监听，向epoll注册fd

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport)
  : loop_(loop),
    acceptSocket_(sockets::createNonblockingOrDie()), // 创建listenfd
    acceptChannel_(loop, acceptSocket_.fd()), // 创建listenfd对应的Channel
    listenning_(false),
    idleFd_(::open("/dev/null", O_RDONLY | O_CLOEXEC)) // 打开一个空的fd，用于占位
{
  assert(idleFd_ >= 0);
  acceptSocket_.setReuseAddr(true); // 复用addr
  acceptSocket_.setReusePort(reuseport); // 复用port
  acceptSocket_.bindAddress(listenAddr); // 绑定ip和port
  acceptChannel_.setReadCallback(
      boost::bind(&Acceptor::handleRead, this)); //设置Channel的read回调函数
}

Acceptor::~Acceptor()
{
  acceptChannel_.disableAll();
  acceptChannel_.remove();
  ::close(idleFd_);
}

// 监听fd
void Acceptor::listen()
{
  loop_->assertInLoopThread();
  listenning_ = true;
  acceptSocket_.listen();
  acceptChannel_.enableReading(); // 开始在epoll中监听read事件
}

// 当epoll监听到listenfd时，开始执行此函数
void Acceptor::handleRead()
{
  loop_->assertInLoopThread();
  InetAddress peerAddr;
  //FIXME loop until no more
  int connfd = acceptSocket_.accept(&peerAddr);
  if (connfd >= 0)
  {
    // string hostport = peerAddr.toIpPort();
    // LOG_TRACE << "Accepts of " << hostport;
    if (newConnectionCallback_)
    {
      // 执行创建连接时的操作，猜测是保存fd，创建TcpConnection之类，然后是将
      // tcp连接分配给其他线程
      newConnectionCallback_(connfd, peerAddr);
    }
    else
    {
      sockets::close(connfd);
    }
  }
  else
  {
    // 这里处理fd达到上限有一个技巧，就是先占住一个空的fd，然后当fd满的时候，先关闭此占位fd，然后
    // 迅速接受新的tcp连接，然后关闭它，然后再次打开此fd
    // 这样的好处是能够及时通知客户端，服务器的fd已经满。
    // 事实上，这里还可以提供给用户一个回调函数，提供fd满时的更具体信息
    LOG_SYSERR << "in Acceptor::handleRead";
    // Read the section named "The special problem of
    // accept()ing when you can't" in libev's doc.
    // By Marc Lehmann, author of livev.
    if (errno == EMFILE) // fd的数目达到上限
    {
      ::close(idleFd_); // 关闭占位的fd
      idleFd_ = ::accept(acceptSocket_.fd(), NULL, NULL); //接收此链接，然后马上关闭
      ::close(idleFd_);
      idleFd_ = ::open("/dev/null", O_RDONLY | O_CLOEXEC); // 重新打开此fd
    }
  }
}

