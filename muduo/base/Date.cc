// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/Date.h>
#include <stdio.h>  // snprintf

namespace muduo
{
namespace detail
{

char require_32_bit_integer_at_least[sizeof(int) >= sizeof(int32_t) ? 1 : -1];

// algorithm and explanation see:
// http://www.faqs.org/faqs/calendars/faq/part2/
// http://blog.csdn.net/Solstice


// 这个算法将年月日对应的日期，换算成该日期对应的儒略日
int getJulianDayNumber(int year, int month, int day)
{
  (void) require_32_bit_integer_at_least; // no warning please
  int a = (14 - month) / 12;
  int y = year + 4800 - a;
  int m = month + 12 * a - 3;
  return day + (153*m + 2) / 5 + y*365 + y/4 - y/100 + y/400 - 32045;
}

// 这个算法则是将儒略日换算成公历的日期
struct Date::YearMonthDay getYearMonthDay(int julianDayNumber)
{
  int a = julianDayNumber + 32044;
  int b = (4 * a + 3) / 146097;
  int c = a - ((b * 146097) / 4);
  int d = (4 * c + 3) / 1461;
  int e = c - ((1461 * d) / 4);
  int m = (5 * e + 2) / 153;
  Date::YearMonthDay ymd;
  ymd.day = e - ((153 * m + 2) / 5) + 1;
  ymd.month = m + 3 - 12 * (m / 10);
  ymd.year = b * 100 + d - 4800 + (m / 10);
  return ymd;
}
}

// 这个常量保存的是1970-01-01对应的儒略日
// 其他日期的儒略日与之想减，就是两个日期相差的天数
const int Date::kJulianDayOf1970_01_01 = detail::getJulianDayNumber(1970, 1, 1);
}

using namespace muduo;
using namespace muduo::detail;

Date::Date(int y, int m, int d)
  : julianDayNumber_(getJulianDayNumber(y, m, d))
{
}

Date::Date(const struct tm& t)
  : julianDayNumber_(getJulianDayNumber(
        t.tm_year+1900,
        t.tm_mon+1,
        t.tm_mday))
{
}

//将Date格式化为2015-01-28
string Date::toIsoString() const
{
  char buf[32];
  YearMonthDay ymd(yearMonthDay());
  snprintf(buf, sizeof buf, "%4d-%02d-%02d", ymd.year, ymd.month, ymd.day);
  return buf;
}

// 将本日期转化为一个年月日对象
Date::YearMonthDay Date::yearMonthDay() const
{
  return getYearMonthDay(julianDayNumber_);
}

