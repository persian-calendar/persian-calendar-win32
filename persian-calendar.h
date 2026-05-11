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

typedef struct { unsigned year; unsigned month; unsigned day; } persian_date_t;
inline persian_date_t gregorian_to_persian(unsigned gy, unsigned gm, unsigned gd) {
  unsigned days;
  {
    unsigned gy2 = (gm > 2) ? gy + 1 : gy;
    static const unsigned g_d_m[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    days = 355666 + (365 * gy) + (gy2 + 3) / 4 - (gy2 + 99) / 100 + (gy2 + 399) / 400 + gd + g_d_m[(gm - 1) % 12];
  }
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

typedef struct { unsigned year; unsigned month; unsigned day; } gregorian_date_t;
inline gregorian_date_t persian_to_gregorian(unsigned jy, unsigned jm, unsigned jd) {
  jy += 1595;
  long days = -355668 + (365 * jy) + (((int)(jy / 33)) * 8) + ((int)(((jy % 33) + 3) / 4)) + jd + ((jm < 7) ? (jm - 1) * 31 : ((jm - 7) * 30) + 186);
  unsigned gy = 400 * ((int)(days / 146097));
  days %= 146097;
  if (days > 36524) {
    gy += 100 * ((int)(--days / 36524));
    days %= 36524;
    if (days >= 365) days++;
  }
  gy += 4 * ((int)(days / 1461));
  days %= 1461;
  if (days > 365) {
    gy += (int)((days - 1) / 365);
    days = (days - 1) % 365;
  }
  unsigned gd = days + 1;
  unsigned gm;
  {
    int sal_a[13] = {0, 31, ((gy % 4 == 0 && gy % 100 != 0) || (gy % 400 == 0)) ? 29 : 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (gm = 0; gm < 13 && gd > sal_a[gm]; gm++) gd -= sal_a[gm];
  }
  gregorian_date_t result;
  result.year = gy;
  result.month = gm;
  result.day = gd;
  return result;
}
