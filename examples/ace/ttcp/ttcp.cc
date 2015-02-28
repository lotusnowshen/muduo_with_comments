#include <examples/ace/ttcp/common.h>

#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpServer.h>

#include <boost/bind.hpp>

#include <stdio.h>

using namespace muduo;
using namespace muduo::net;

EventLoop* g_loop;

/*
  非阻塞的ttcp，主要是处理好上下文
*/

// 上下文信息
struct Context
{
  int count; // 发送或者接收的消息数目
  int64_t bytes;  // 发送或者接收的字节数
  SessionMessage session; // 控制信息
  Buffer output;  // 暂存报文

  Context()
    : count(0),
      bytes(0)
  {
    session.number = 0;
    session.length = 0;
  }
};

/////////////////////////////////////////////////////////////////////
// T R A N S M I T
/////////////////////////////////////////////////////////////////////

namespace trans
{

void onConnection(const Options& opt, const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    printf("connected\n");
    Context context;
    context.count = 1;  // 当前发送的消息条数
    context.bytes = opt.length; // 当前发送的字节数
    context.session.number = opt.number; // 报文数目
    context.session.length = opt.length; // 每条报文的长度
    context.output.appendInt32(opt.length); // 先放入一个长度
    context.output.ensureWritableBytes(opt.length); // 确保buffer空间足够
    // 填充满buffer，此时的buffer恰好为一个报文
    for (int i = 0; i < opt.length; ++i)
    {
      context.output.beginWrite()[i] = "0123456789ABCDEF"[i % 16];
    }
    // 上面仅仅是对字节赋值，这一步才是真正写入
    context.output.hasWritten(opt.length);
    conn->setContext(context); // 保存在tcp上下文中

    SessionMessage sessionMessage = { 0, 0 };
    sessionMessage.number = htonl(opt.number);
    sessionMessage.length = htonl(opt.length);
    // 发送报文信息
    conn->send(&sessionMessage, sizeof(sessionMessage));
    // 发送第一条报文
    conn->send(context.output.toStringPiece());
  }
  else
  {
    const Context& context = boost::any_cast<Context>(conn->getContext());
    LOG_INFO << "payload bytes " << context.bytes;
    conn->getLoop()->quit();
  }
}

// 接收ACK 同时发送新的报文
void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
  // 取出上下文
  Context* context = boost::any_cast<Context>(conn->getMutableContext());
  // 必须使用while
  while (buf->readableBytes() >= sizeof(int32_t))
  {
    int32_t length = buf->readInt32();
    // ACK中的长度正确
    if (length == context->session.length)
    {
      // 还没全部接收完
      if (context->count < context->session.number)
      {
        // 在发送一条
        conn->send(context->output.toStringPiece());
        // 更新发送消息的数目和字节数
        ++context->count;
        context->bytes += length;
      }
      else
      {
        // 全部接受完
        conn->shutdown();
        break;
      }
    }
    else
    {
      conn->shutdown();
      break;
    }
  }
}

}

void transmit(const Options& opt)
{
  InetAddress addr(opt.port);
  if (!InetAddress::resolve(opt.host, &addr))
  {
    LOG_FATAL << "Unable to resolve " << opt.host;
  }
  muduo::Timestamp start(muduo::Timestamp::now());
  EventLoop loop;
  g_loop = &loop;
  TcpClient client(&loop, addr, "TtcpClient");
  client.setConnectionCallback(
      boost::bind(&trans::onConnection, opt, _1));
  client.setMessageCallback(
      boost::bind(&trans::onMessage, _1, _2, _3));
  client.connect();
  loop.loop();
  // 统计性能
  double elapsed = timeDifference(muduo::Timestamp::now(), start);
  double total_mb = 1.0 * opt.length * opt.number / 1024 / 1024;
  printf("%.3f MiB transferred\n%.3f MiB/s\n", total_mb, total_mb / elapsed);
}

/////////////////////////////////////////////////////////////////////
// R E C E I V E
/////////////////////////////////////////////////////////////////////

namespace receiving
{

// 服务器是被动接收的一方，所以这里只设置上下文即可
void onConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    Context context;
    conn->setContext(context);
  }
  else
  {
    const Context& context = boost::any_cast<Context>(conn->getContext());
    LOG_INFO << "payload bytes " << context.bytes;
    conn->getLoop()->quit();
  }
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
  // 这里使用while
  while (buf->readableBytes() >= sizeof(int32_t))
  {
    Context* context = boost::any_cast<Context>(conn->getMutableContext());
    SessionMessage& session = context->session;
    // session未初始化，说明发送过来的是session
    if (session.number == 0 && session.length == 0)
    {
      if (buf->readableBytes() >= sizeof(SessionMessage))
      {
        session.number = buf->readInt32();
        session.length = buf->readInt32();
        // buffer中保存报文的长度
        context->output.appendInt32(session.length);
        printf("receive number = %d\nreceive length = %d\n",
               session.number, session.length);
      }
      else
      {
        break;
      }
    }
    else
    {
      // 消息的总长度
      const unsigned total_len = session.length + static_cast<int>(sizeof(int32_t));

      const int32_t length = buf->peekInt32();
      if (length == session.length)
      {
        if (buf->readableBytes() >= total_len)
        {
          buf->retrieve(total_len);
          // 发送ACK 此时output仅存储了一个长度
          conn->send(context->output.toStringPiece());
          // 更新接收的字节数和消息数目
          ++context->count;
          context->bytes += length;
          // 接收完毕
          if (context->count >= session.number)
          {
            conn->shutdown();
            break;
          }
        }
        else
        {
          break;
        }
      }
      else
      {
        printf("wrong length %d\n", length);
        conn->shutdown();
        break;
      }
    }
  }
}

}

void receive(const Options& opt)
{
  EventLoop loop;
  g_loop = &loop;
  InetAddress listenAddr(opt.port);
  TcpServer server(&loop, listenAddr, "TtcpReceive");
  server.setConnectionCallback(
       boost::bind(&receiving::onConnection, _1));
  server.setMessageCallback(
      boost::bind(&receiving::onMessage, _1, _2, _3));
  server.start();
  loop.loop();
}
