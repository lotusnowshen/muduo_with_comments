// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_ATOMIC_H
#define MUDUO_BASE_ATOMIC_H

#include <boost/noncopyable.hpp>
#include <stdint.h>

//原子数据类模板，该类的所有操作都是线程安全的
namespace muduo
{

namespace detail
{
template<typename T>
class AtomicIntegerT : boost::noncopyable //值语义
{
 public:
  AtomicIntegerT()
    : value_(0)
  {
  }

  // uncomment if you need copying and assignment
  //
  // AtomicIntegerT(const AtomicIntegerT& that)
  //   : value_(that.get())
  // {}
  //
  // AtomicIntegerT& operator=(const AtomicIntegerT& that)
  // {
  //   getAndSet(that.get());
  //   return *this;
  // }

  T get()
  {
    // in gcc >= 4.7: __atomic_load_n(&value_, __ATOMIC_SEQ_CST)
    //type __sync_val_compare_and_swap (type *ptr, type oldval, type newval, ...)
    //测试ptr与oldval是否相同，如果相同，就把newval写入ptr
    //返回操作之前的值
    //这里的作用就是返回value_的值
    return __sync_val_compare_and_swap(&value_, 0, 0);
  }

  T getAndAdd(T x)
  {
    // in gcc >= 4.7: __atomic_fetch_add(&value_, x, __ATOMIC_SEQ_CST)
    //value加上x的值，然后返回value的旧值
    return __sync_fetch_and_add(&value_, x);
  }

  T addAndGet(T x)
  {
    //这里调用getAndAdd，保证了原子性，然后+x，返回的就是相加之后的value
    return getAndAdd(x) + x;
  }

  T incrementAndGet()
  {
    //++i 获取的是+1之后的值
    return addAndGet(1);
  }

  T decrementAndGet()
  {
    //--i 获取-1之后的值
    return addAndGet(-1);
  }

  void add(T x)
  {

    //__sync_fetch_and_add
    getAndAdd(x);
  }

  void increment()
  {
    //最终调用__sync_fetch_and_add(&value_, 1)
    incrementAndGet();
  }

  void decrement()
  {
    //同上
    decrementAndGet();
  }

  T getAndSet(T newValue)
  {
    // in gcc >= 4.7: __atomic_store_n(&value, newValue, __ATOMIC_SEQ_CST)
    //type __sync_lock_test_and_set (type *ptr, type value, ...)
    //将*ptr设为value并返回*ptr操作之前的值
    return __sync_lock_test_and_set(&value_, newValue);
  }

 private:
  volatile T value_;  //volatile防止进行寄存器优化
};
}

typedef detail::AtomicIntegerT<int32_t> AtomicInt32;
typedef detail::AtomicIntegerT<int64_t> AtomicInt64;
}

#endif  // MUDUO_BASE_ATOMIC_H
