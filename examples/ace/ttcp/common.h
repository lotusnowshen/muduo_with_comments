#pragma once

#include <string>
#include <stdint.h>

struct Options
{
  uint16_t port;  // 端口
  int length;     // 每条报文长度
  int number;     // 报文数目
  bool transmit, receive, nodelay;
  // 根据前两项决定是客户端还是服务器
  // transmit 客户端
  // receive 服务器
  // nodelay 禁用nagle算法
  std::string host;
  Options()
    : port(0), length(0), number(0),
      transmit(false), receive(false), nodelay(false)
  {
  }
};

// 解析命令行
bool parseCommandLine(int argc, char* argv[], Options* opt);
// 解析域名
struct sockaddr_in resolveOrDie(const char* host, uint16_t port);

struct SessionMessage
{
  int32_t number; // 报文的数目
  int32_t length; // 每条报文的长度
} __attribute__ ((__packed__));

struct PayloadMessage
{
  int32_t length; // 报文的长度
  char data[0]; // 变长数组，可以在运行期间决定
};

// 客户端发送数据
void transmit(const Options& opt);

// 服务器接收数据
void receive(const Options& opt);
