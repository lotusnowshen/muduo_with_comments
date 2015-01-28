// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_DATE_H
#define MUDUO_BASE_DATE_H

#include <muduo/base/copyable.h>
#include <muduo/base/Types.h>

struct tm;

namespace muduo
{

///
/// Date in Gregorian calendar.
///
/// This class is immutable.
/// It's recommended to pass it by value, since it's passed in register on x64.
///

// 以下内容来自维基百科：http://zh.wikipedia.org/wiki/%E5%84%92%E7%95%A5%E6%97%A5

// 儒略日（Julian day）是指由公元前4713年1月1日，协调世界时中午12时开始所经过的天数，多为天文学家采用，用以作为天文学的单一历法，把不同历法的年表统一起来。

// 儒略日是一种不用年月的长期纪日法，简写为JD。是由荷兰纪年学家史迦利日（Joseph Justus Scliger 1540年-1609年）在1583年所创，这名称是为了纪念他的父亲——意大利学者Julius Caesar Scaliger（1484年-1558年）。

// 以儒略日计日是为方便计算年代相隔久远或不同历法的两事件所间隔的日数。

class Date : public muduo::copyable
          // public boost::less_than_comparable<Date>,
          // public boost::equality_comparable<Date>
{
 public:

  // 表示年月日
  struct YearMonthDay
  {
    int year; // [1900..2500]
    int month;  // [1..12]
    int day;  // [1..31]
  };

  // 一周是七天
  static const int kDaysPerWeek = 7;
  static const int kJulianDayOf1970_01_01;

  ///
  /// Constucts an invalid Date.
  ///
  Date()
    : julianDayNumber_(0)  // 构造一个无效的日期
  {}

  ///
  /// Constucts a yyyy-mm-dd Date.
  ///
  /// 1 <= month <= 12
  Date(int year, int month, int day);

  ///
  /// Constucts a Date from Julian Day Number.
  ///
  explicit Date(int julianDayNum)
    : julianDayNumber_(julianDayNum)
  {}

  ///
  /// Constucts a Date from struct tm
  ///
  explicit Date(const struct tm&);

  // default copy/assignment/dtor are Okay

  void swap(Date& that)
  {
    std::swap(julianDayNumber_, that.julianDayNumber_);
  }

  bool valid() const { return julianDayNumber_ > 0; }

  ///
  /// Converts to yyyy-mm-dd format.
  ///
  string toIsoString() const;

  // 使用julianDayNumber_ 构造一个年月日对象，以便于提取其中的year month day
  struct YearMonthDay yearMonthDay() const;

  int year() const
  {
    return yearMonthDay().year;
  }

  int month() const
  {
    return yearMonthDay().month;
  }

  int day() const
  {
    return yearMonthDay().day;
  }

  // [0, 1, ..., 6] => [Sunday, Monday, ..., Saturday ]
  // 一周的第几天
  int weekDay() const
  {
    return (julianDayNumber_+1) % kDaysPerWeek;
  }

  int julianDayNumber() const { return julianDayNumber_; }

 private:
  int julianDayNumber_; //本日期对应的儒略日
};

inline bool operator<(Date x, Date y)
{
  return x.julianDayNumber() < y.julianDayNumber();
}

inline bool operator==(Date x, Date y)
{
  return x.julianDayNumber() == y.julianDayNumber();
}

}
#endif  // MUDUO_BASE_DATE_H
