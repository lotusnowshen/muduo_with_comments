#ifndef MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
#define MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H

#include <muduo/base/Logging.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Endian.h>
#include <muduo/net/TcpConnection.h>

#include <boost/function.hpp>
#include <boost/noncopyable.hpp>

class LengthHeaderCodec : boost::noncopyable
{
 public:
  typedef boost::function<void (const muduo::net::TcpConnectionPtr&,
                                const muduo::string& message,
                                muduo::Timestamp)> StringMessageCallback;

  explicit LengthHeaderCodec(const StringMessageCallback& cb)
    : messageCallback_(cb)
  {
  }

  // 这个是message的回调，它负责数据的分包
  void onMessage(const muduo::net::TcpConnectionPtr& conn,
                 muduo::net::Buffer* buf,
                 muduo::Timestamp receiveTime)
  {
    // 这里必须使用while，不能使用if（思考为什么？）
    while (buf->readableBytes() >= kHeaderLen) // kHeaderLen == 4
    {
      // 先查看4个字节，但是不能将其从buffer中取走（为什么？）
      // FIXME: use Buffer::peekInt32()
      const void* data = buf->peek();
      int32_t be32 = *static_cast<const int32_t*>(data); // SIGBUS
      // 转化为本地主机序
      const int32_t len = muduo::net::sockets::networkToHost32(be32);
      if (len > 65536 || len < 0)
      {
        LOG_ERROR << "Invalid length " << len;
        conn->shutdown();  // FIXME: disable reading
        break;
      }
      else if (buf->readableBytes() >= len + kHeaderLen) // buffer中有一条完整的信息
      {
        buf->retrieve(kHeaderLen); // 从buffer中取走开头四个字节
        muduo::string message(buf->peek(), len); // 收取正式的一条消息
        // 接收到一条完整的消息，需要使用messageCallback_，由ChatServer去处理
        messageCallback_(conn, message, receiveTime);
        buf->retrieve(len); // 取走整条消息
      }
      else
      {
        break;
      }
    }
  }

  // FIXME: TcpConnectionPtr
  void send(muduo::net::TcpConnection* conn,
            const muduo::StringPiece& message)
  {
    muduo::net::Buffer buf;
    // 将消息放入缓冲区
    buf.append(message.data(), message.size());
    int32_t len = static_cast<int32_t>(message.size());
    int32_t be32 = muduo::net::sockets::hostToNetwork32(len);
    // 在buffer前添加消息的长度
    buf.prepend(&be32, sizeof be32);
    conn->send(&buf);
  }

 private:
  StringMessageCallback messageCallback_;
  const static size_t kHeaderLen = sizeof(int32_t);
};

#endif  // MUDUO_EXAMPLES_ASIO_CHAT_CODEC_H
