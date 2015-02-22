// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is an internal header file, you should not include this.

#ifndef MUDUO_NET_ACCEPTOR_H
#define MUDUO_NET_ACCEPTOR_H

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

#include <muduo/net/Channel.h>
#include <muduo/net/Socket.h>

namespace muduo
{
namespace net
{

class EventLoop;
class InetAddress;

// 这个类主要是封装了listenfd对应的Channel。
// 它的任务就是创建listenfd，然后accept新的tcp连接是它最核心的职责
// 所以这个类已经具备了一个tcp服务器大部分的能力
// 但这个类只负责接受tcp连接，并不负责tcp连接的分配，这个是Acceptor的上层-TcpServer的任务

///
/// Acceptor of incoming TCP connections.
///
class Acceptor : boost::noncopyable
{
 public:
  typedef boost::function<void (int sockfd,
                                const InetAddress&)> NewConnectionCallback;

  Acceptor(EventLoop* loop, const InetAddress& listenAddr, bool reuseport);
  ~Acceptor();

  void setNewConnectionCallback(const NewConnectionCallback& cb)
  { newConnectionCallback_ = cb; }

  bool listenning() const { return listenning_; }
  void listen();

 private:
  void handleRead();

  EventLoop* loop_; // 所在的EventLoop
  Socket acceptSocket_; // 对应的listenfd
  Channel acceptChannel_; // 对应的Channel
  NewConnectionCallback newConnectionCallback_; // 创建连接时的回调
  bool listenning_;
  int idleFd_; // 占位fd，用于fd满的情况
};

}
}

#endif  // MUDUO_NET_ACCEPTOR_H
