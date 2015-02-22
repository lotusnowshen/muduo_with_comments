// Copyright 2010, Shuo Chen.  All rights reserved.
// http://code.google.com/p/muduo/
//
// Use of this source code is governed by a BSD-style license
// that can be found in the License file.

// Author: Shuo Chen (chenshuo at chenshuo dot com)
//
// This is a public header file, it must only include public header files.

#ifndef MUDUO_NET_TCPCONNECTION_H
#define MUDUO_NET_TCPCONNECTION_H

#include <muduo/base/StringPiece.h>
#include <muduo/base/Types.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/InetAddress.h>

#include <boost/any.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

// struct tcp_info is in <netinet/tcp.h>
struct tcp_info;

namespace muduo
{
namespace net
{

class Channel;
class EventLoop;
class Socket;

///
/// TCP connection, for both client and server usage.
///
/// This is an interface class, so don't expose too much details.
class TcpConnection : boost::noncopyable,
                      public boost::enable_shared_from_this<TcpConnection>
{
 public:
  /// Constructs a TcpConnection with a connected sockfd
  ///
  /// User should not create this object.
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;
  string getTcpInfoString() const;

  // void send(string&& message); // C++11
  void send(const void* message, int len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void forceCloseWithDelay(double seconds);
  void setTcpNoDelay(bool on);

  // 设置TCP上下文 boost::any http://www.boost.org/doc/libs/1_57_0/doc/html/any.html
  void setContext(const boost::any& context)
  { context_ = context; }

  // 获取TCP上下文
  const boost::any& getContext() const
  { return context_; }

  boost::any* getMutableContext()
  { return &context_; }

  // 设置连接建立和关闭时的回调函数
  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  // 设置收到消息时的回调函数
  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  // 设置当成功将所有数据写入对方内核缓冲区时的回调函数
  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  // 设置高水位回调函数和高水位值，当缓冲区的size达到highWaterMark时触发此请求
  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  // 返回输入缓冲区和输出缓冲区的指针
  /// Advanced interface
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  Buffer* outputBuffer()
  { return &outputBuffer_; }

  /// Internal use only.
  // 设置TCP连接关闭的回调函数，仅仅在内部使用，这个是真正关闭TCP的动作，不能由用户指定
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // 以下两个函数，提供给TcpServer使用
  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime); // 处理read事件，receiveTime指的是poll调用返回的事件 
  void handleWrite(); // 处理写事件
  void handleClose(); // 处理连接关闭事件
  void handleError(); // 处理错误事件
  // void sendInLoop(string&& message);
  // 因为muduo中的IO不能跨线程，所以发送msg必须在EventLoop中，所以这里的sendInLoop底层
  // 有判断，如果跨线程，则将其放入队列，这几个函数供send调用
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  void setState(StateE s) { state_ = s; } // 设置TCP连接的状态

  EventLoop* loop_;   // 处理该TCP连接的EventLoop，该EventLoop内部的epoll监听TCP连接对应的fd
  const string name_; // 连接的名字
  StateE state_;  // FIXME: use atomic variable // 本条TCP连接的状态
  // we don't expose those classes to client.
  boost::scoped_ptr<Socket> socket_; // TCP连接的fd所在的socket对象 fd的关闭由它决定
  boost::scoped_ptr<Channel> channel_; // TCP连接fd对应的Channel，将其放入EventLoop
  const InetAddress localAddr_; // TCP连接中本地的ip地址和端口号
  const InetAddress peerAddr_; // TCP连接中对方的ip地址和端口号
  ConnectionCallback connectionCallback_;  // 连接建立和关闭时的回调函数
  MessageCallback messageCallback_; // 收到消息时的回调函数 
  WriteCompleteCallback writeCompleteCallback_; // 消息写入对方缓冲区时的回调函数
  HighWaterMarkCallback highWaterMarkCallback_; // 高水位回调函数
  CloseCallback closeCallback_; // 关闭TCP连接的回调函数
  size_t highWaterMark_;    // 高水位标记
  Buffer inputBuffer_;  // TCP连接的输入缓冲区，从连接中读取输入然后存入
  Buffer outputBuffer_; // FIXME: use list<Buffer> as output buffer. TCP的输出缓冲区，要发送的数据保存在这里
  boost::any context_;  // TCP连接的上下文，一般用于处理多次消息相互存在关联的情形，例如文件发送
  // FIXME: creationTime_, lastReceiveTime_
  //        bytesReceived_, bytesSent_
};

typedef boost::shared_ptr<TcpConnection> TcpConnectionPtr; // 使用引用计数型智能指针管理TCP连接对象的生存周期
// 当TCP连接关闭时，必须将其他拥有该TCP智能指针的对象销毁，最后该TCP才能正确关闭

}
}

#endif  // MUDUO_NET_TCPCONNECTION_H
