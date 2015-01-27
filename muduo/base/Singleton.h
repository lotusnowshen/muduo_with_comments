// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

// 这个版本的单例模式，利用pthread_once实现了对象的唯一性
namespace muduo
{

namespace detail
{
// This doesn't detect inherited member functions!
// http://stackoverflow.com/questions/1966362/sfinae-to-check-for-inherited-member-functions
template<typename T>
struct has_no_destroy
{
  // typeof 是GCC提供的关键字
  template <typename C> static char test(typeof(&C::no_destroy)); // or decltype in C++11
  template <typename C> static int32_t test(...);
  const static bool value = sizeof(test<T>(0)) == 1;
};
}
// 对上面的has_no_destroy的解释如下：
// 对于detail::has_no_destroy<T>::value
// 如果是class类型，那么模板推断决议，将test<T>(0)推断至第一个版本，sizeof(test<T>(0))为1
// value为true
// 如果是int、double等类型，推断至第二个版本，value为False
// 所以对于class类型，detail::has_no_destroy<T>::value测试为True
// 对于int则测试为False

template<typename T>
class Singleton : boost::noncopyable
{
 public:
  static T& instance()
  {
    pthread_once(&ponce_, &Singleton::init);
    return *value_;
  }

 private:
  Singleton();
  ~Singleton();

  static void init()
  {
    value_ = new T();
    //参见上面的解释，对于普通类型不注册析构函数
    if (!detail::has_no_destroy<T>::value)
    {
      ::atexit(destroy);
    }
  }

  static void destroy()
  {
    // 检测T的类型是否完整
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    T_must_be_complete_type dummy; (void) dummy;

    delete value_;
  }

 private:
  static pthread_once_t ponce_;
  static T*             value_;
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT;

template<typename T>
T* Singleton<T>::value_ = NULL;

}
#endif

