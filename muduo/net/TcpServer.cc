// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/net/TcpServer.h>

#include <muduo/base/Logging.h>
#include <muduo/net/Acceptor.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/EventLoopThreadPool.h>
#include <muduo/net/SocketsOps.h>

#include <boost/bind.hpp>

#include <stdio.h>  // snprintf

using namespace muduo;
using namespace muduo::net;

TcpServer::TcpServer(EventLoop* loop,
                     const InetAddress& listenAddr,
                     const string& nameArg,
                     Option option)
  : loop_(CHECK_NOTNULL(loop)),
    hostport_(listenAddr.toIpPort()),
    name_(nameArg),
    acceptor_(new Acceptor(loop, listenAddr, option == kReusePort)),
    threadPool_(new EventLoopThreadPool(loop)),
    connectionCallback_(defaultConnectionCallback),
    messageCallback_(defaultMessageCallback),
    nextConnId_(1)
{
  // 将新建tcp连接的操作注册到acceptor中
  acceptor_->setNewConnectionCallback(
      boost::bind(&TcpServer::newConnection, this, _1, _2));
}

TcpServer::~TcpServer()
{
  loop_->assertInLoopThread();
  LOG_TRACE << "TcpServer::~TcpServer [" << name_ << "] destructing";

  for (ConnectionMap::iterator it(connections_.begin());
      it != connections_.end(); ++it)
  {
    TcpConnectionPtr conn = it->second;
    it->second.reset();
    conn->getLoop()->runInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));
    conn.reset();
  }
}

// 设置线程数目，这一步就可以决定采用的是单线程reactor，还是多线程
void TcpServer::setThreadNum(int numThreads)
{
  assert(0 <= numThreads);
  threadPool_->setThreadNum(numThreads);
}

// tcpServer的启动流程：
// 1. 启动线程池的Thread
// 2. 开始listen
// 3. 开启acceptor对应的read事件，此时开始在epoll中监听listenfd的read事件

void TcpServer::start()
{
  if (started_.getAndSet(1) == 0)
  {
    // 启动线程
    threadPool_->start(threadInitCallback_);

    assert(!acceptor_->listenning());
    // 开始listen
    loop_->runInLoop(
        boost::bind(&Acceptor::listen, get_pointer(acceptor_)));
  }
}

// muduo接受新连接的流程：
// 1. 底层accept一个新的fd
// 2. 从线程池中选取一个loop线程
// 3. 生成新的TcpConnection
// 4. 设置好各种回调函数
// 5. 在该loop执行tcp连接建立时的回调操作
//   包括tcp建立时的用户回调操作
//   开始监听此客户fd的read事件等

// 这是个回调函数，用于Acceptor接受新连接后的处理
// sockfd  新建tcp连接的fd
// peerAddr tcp的客户端地址
void TcpServer::newConnection(int sockfd, const InetAddress& peerAddr)
{
  loop_->assertInLoopThread();
  // 从线程池中选取一个loop
  EventLoop* ioLoop = threadPool_->getNextLoop();
  // 生成tcp连接的名称
  char buf[32];
  snprintf(buf, sizeof buf, ":%s#%d", hostport_.c_str(), nextConnId_);
  ++nextConnId_;
  string connName = name_ + buf;

  LOG_INFO << "TcpServer::newConnection [" << name_
           << "] - new connection [" << connName
           << "] from " << peerAddr.toIpPort();
  InetAddress localAddr(sockets::getLocalAddr(sockfd));
  // FIXME poll with zero timeout to double confirm the new connection
  // FIXME use make_shared if necessary
  // 新建tcp连接
  TcpConnectionPtr conn(new TcpConnection(ioLoop,
                                          connName,
                                          sockfd,
                                          localAddr,
                                          peerAddr));
  // 保存该conn，这里非常关键，这一步保证了conn的引用计数最低为1
  connections_[connName] = conn;
  conn->setConnectionCallback(connectionCallback_);
  conn->setMessageCallback(messageCallback_);
  conn->setWriteCompleteCallback(writeCompleteCallback_);
  // TCP关闭时的回调函数
  conn->setCloseCallback(
      boost::bind(&TcpServer::removeConnection, this, _1)); // FIXME: unsafe
  // 在loop线程中执行建立tcp连接的流程，主要是设置tcp状态，以及执行tcp建立的回调函数
  ioLoop->runInLoop(boost::bind(&TcpConnection::connectEstablished, conn));
}

// 关闭连接时执行的操作
void TcpServer::removeConnection(const TcpConnectionPtr& conn)
{
  // FIXME: unsafe
  loop_->runInLoop(boost::bind(&TcpServer::removeConnectionInLoop, this, conn));
}

// muduo被动关闭tcp连接的流程： 
// 1. read收到0，或者epoll监听到HUP事件
// 2. 调用conn中的handleClose函数
// 3. 停止监听所有的事件
// 4. 执行用户的close逻辑
// 5. 执行close回调函数：
// 6. 执行TcpServer中的removeConnection（removeConnectionInLoop）
// 7. connections_中移除conn，引用计数-1
// 8. 执行TcpTcpConnection中connectDestroyed，将Channel指针从loop中移除
// 在上述关闭过程中，为什么需要用到TcpServer中的函数，原因是connections_这个数据结构的存在
// 为了维持TcpConnection的生存期，需要将ptr保存在connections_中，当tcp关闭时，
// 也必须去处理这个数据结构

void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn)
{
  loop_->assertInLoopThread();
  LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
           << "] - connection " << conn->name();
  // 这里将conn的引用计数加一，假设其他地方没有持有conn的ptr，那么conn会被析构
  size_t n = connections_.erase(conn->name());
  (void)n;
  assert(n == 1);
  // 取出该tcp连接所在的loop
  EventLoop* ioLoop = conn->getLoop();
  // 执行该conn关闭时的回调函数
  ioLoop->queueInLoop(
      boost::bind(&TcpConnection::connectDestroyed, conn));
}

