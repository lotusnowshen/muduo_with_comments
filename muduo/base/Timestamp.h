#ifndef MUDUO_BASE_TIMESTAMP_H
#define MUDUO_BASE_TIMESTAMP_H

#include <muduo/base/copyable.h>
#include <muduo/base/Types.h>

#include <boost/operators.hpp>

namespace muduo
{

///
/// Time stamp in UTC, in microseconds resolution.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///
// 时间戳类，值语义，同时继承boost的less_than_comparable，
// 该类根据<和==可以自动生成 > >= <= !=四个运算符
// 时间戳传参使用pass-by-value即可
class Timestamp : public muduo::copyable,
                  public boost::less_than_comparable<Timestamp>
{
 public:
  ///
  /// Constucts an invalid Timestamp.
  ///
  Timestamp()
    : microSecondsSinceEpoch_(0)
  {
  }

  ///
  /// Constucts a Timestamp at specific time
  ///
  /// @param microSecondsSinceEpoch
  explicit Timestamp(int64_t microSecondsSinceEpoch);

  void swap(Timestamp& that)
  {
    std::swap(microSecondsSinceEpoch_, that.microSecondsSinceEpoch_);
  }

  // default copy/assignment/dtor are Okay

  // 1422366082.375828
  string toString() const;
  // 20150127 21:46:45:376742
  string toFormattedString(bool showMicroseconds = true) const;

  // 该时间戳对象是否是有效的
  bool valid() const { return microSecondsSinceEpoch_ > 0; }

  // for internal usage.
  // 返回精确的微秒时间戳
  int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }
  // 返回以s为单位的普通时间戳
  time_t secondsSinceEpoch() const
  { return static_cast<time_t>(microSecondsSinceEpoch_ / kMicroSecondsPerSecond); }

  ///
  /// Get time of now.
  ///
  // 获取当前时间
  static Timestamp now();
  // 获取一个不可用的时间戳
  static Timestamp invalid();

  static const int kMicroSecondsPerSecond = 1000 * 1000;

 private:
  int64_t microSecondsSinceEpoch_;  //以微秒为单位
};

inline bool operator<(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() < rhs.microSecondsSinceEpoch();
}

inline bool operator==(Timestamp lhs, Timestamp rhs)
{
  return lhs.microSecondsSinceEpoch() == rhs.microSecondsSinceEpoch();
}

///
/// Gets time difference of two timestamps, result in seconds.
///
/// @param high, low
/// @return (high-low) in seconds
/// @c double has 52-bit precision, enough for one-microsecond
/// resolution for next 100 years.
// 返回两个时间戳的差，high为较大的时间，返回一个double，整数部分以s为单位
inline double timeDifference(Timestamp high, Timestamp low)
{
  int64_t diff = high.microSecondsSinceEpoch() - low.microSecondsSinceEpoch();
  return static_cast<double>(diff) / Timestamp::kMicroSecondsPerSecond;
}

///
/// Add @c seconds to given timestamp.
///
/// @return timestamp+seconds as Timestamp
///
// 时间戳加上秒数生成新的时间戳
inline Timestamp addTime(Timestamp timestamp, double seconds)
{
  int64_t delta = static_cast<int64_t>(seconds * Timestamp::kMicroSecondsPerSecond);
  return Timestamp(timestamp.microSecondsSinceEpoch() + delta);
}

}
#endif  // MUDUO_BASE_TIMESTAMP_H
