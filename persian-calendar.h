// Modified from https://github.com/SCR-IR/jalaliDate-Cpp/blob/e3f5989/src/converter.cpp
// Have a look at the source for more reliable implementation as that
// is changed here for better or worse.

/**  Gregorian & Jalali (Hijri_Shamsi,Solar) Date Converter Functions
Author: JDF.SCR.IR =>> Download Full Version :  http://jdf.scr.ir/jdf
License: GNU/LGPL _ Open Source & Free :: Version: 2.80 : [2020=1399]
---------------------------------------------------------------------
355746=361590-5844 & 361590=(30*33*365)+(30*8) & 5844=(16*365)+(16/4)
355666=355746-79-1 & 355668=355746-79+1 &  1595=605+990 &  605=621-16
990=30*33 & 12053=(365*33)+(32/4) & 36524=(365*100)+(100/4)-(100/100)
1461=(365*4)+(4/4) & 146097=(365*400)+(400/4)-(400/100)+(400/400)  */

typedef struct date_triplet_t { unsigned year; unsigned month; unsigned day; } persian_date_t, gregorian_date_t;

inline unsigned gregorian_to_days(gregorian_date_t date) {
  unsigned gy2 = (date.month > 2) ? date.year + 1 : date.year;
  static const unsigned g_d_m[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  return 355666 + 365 * date.year + (gy2 + 3) / 4 - (gy2 + 99) / 100 + (gy2 + 399) / 400 + date.day + g_d_m[(date.month - 1) % 12];
}
inline persian_date_t days_to_persian(unsigned days) {
  unsigned year = days / 12053 * 33 - 1595;
  days %= 12053;
  year += days / 1461 * 4;
  days %= 1461;
  if (days > 365) {
    year += (days - 1) / 365;
    days = (days - 1) % 365;
  }

  persian_date_t result;
  result.year = year;
  if (days < 186) {
    result.month = 1 + days / 31;
    result.day = 1 + days % 31;
  } else {
    result.month = 7 + (days - 186) / 30;
    result.day = 1 + (days - 186) % 30;
  }
  return result;
}

inline unsigned persian_to_days(persian_date_t date) {
  unsigned py = date.year + 1595;
  unsigned pm = date.month;
  unsigned pd = date.day;
  return 365 * py + py / 33 * 8 + (py % 33 + 3) / 4 + pd + ((pm < 7) ? (pm - 1) * 31 : (pm - 7) * 30 + 186) - 1;
}
inline gregorian_date_t days_to_gregorian(unsigned days) {
  days -= 355667;
  unsigned gy = days / 146097 * 400;
  days %= 146097;
  if (days > 36524) {
    gy += 100 * (--days / 36524);
    days %= 36524;
    if (days >= 365) ++days;
  }
  gy += 4 * (days / 1461);
  days %= 1461;
  if (days > 365) {
    gy += (days - 1) / 365;
    days = (days - 1) % 365;
  }
  unsigned gd = days + 1;
  unsigned gm;
  {
    unsigned leap_month = (gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0) ? 29 : 28;
    static const unsigned sal_a[13] = {0, 31, 0, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (gm = 0; gm < 13 && gd > (gm == 2 ? leap_month : sal_a[gm % 13]); ++gm)
      gd -= gm == 2 ? leap_month : sal_a[gm % 13];
  }
  gregorian_date_t result;
  result.year = gy;
  result.month = gm;
  result.day = gd;
  return result;
}
