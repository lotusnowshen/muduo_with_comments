// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_EXCEPTION_H
#define MUDUO_BASE_EXCEPTION_H

#include <muduo/base/Types.h>
#include <exception>

namespace muduo
{

// 这个类相对于C++标准库的std::exception主要是提供了打印栈痕迹的功能
// 对于这个类，我们可以这样使用：
// try
// {
//     //
// }
// catch (const Exception& ex)
// {
//     fprintf(stderr, "reason: %s\n", ex.what());
//     fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
//     abort();
// }
// catch (const std::exception& ex)
// {
//     fprintf(stderr, "reason: %s\n", ex.what());
//     abort();
// }
// catch (...)
// {
//     fprintf(stderr, "unknown exception caught \n");
// throw; // rethrow
// }

class Exception : public std::exception
{
 public:
  explicit Exception(const char* what);
  explicit Exception(const string& what);
  virtual ~Exception() throw();
  virtual const char* what() const throw();
  const char* stackTrace() const throw();

 private:
  void fillStackTrace();

  string message_;
  string stack_;
};

}

#endif  // MUDUO_BASE_EXCEPTION_H
