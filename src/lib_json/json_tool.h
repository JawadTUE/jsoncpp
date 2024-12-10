#ifndef LIB_JSONCPP_JSON_TOOL_H_INCLUDED
#define LIB_JSONCPP_JSON_TOOL_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include <json/config.h>
#endif

#ifdef NO_LOCALE_SUPPORT
#define JSONCPP_NO_LOCALE_SUPPORT
#endif

#ifndef JSONCPP_NO_LOCALE_SUPPORT
#include <clocale>
#endif

namespace Json {
static inline char getDecimalPoint() {
#ifdef JSONCPP_NO_LOCALE_SUPPORT
  return '\0';
#else
  struct lconv* lc = localeconv();
  return lc ? *(lc->decimal_point) : '\0';
#endif
}

static inline String codePointToUTF8(unsigned int cp) {
  String result;


  if (cp <= 0x7f) {
    result.resize(1);
    result[0] = static_cast<char>(cp);
  } else if (cp <= 0x7FF) {
    result.resize(2);
    result[1] = static_cast<char>(0x80 | (0x3f & cp));
    result[0] = static_cast<char>(0xC0 | (0x1f & (cp >> 6)));
  } else if (cp <= 0xFFFF) {
    result.resize(3);
    result[2] = static_cast<char>(0x80 | (0x3f & cp));
    result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
    result[0] = static_cast<char>(0xE0 | (0xf & (cp >> 12)));
  } else if (cp <= 0x10FFFF) {
    result.resize(4);
    result[3] = static_cast<char>(0x80 | (0x3f & cp));
    result[2] = static_cast<char>(0x80 | (0x3f & (cp >> 6)));
    result[1] = static_cast<char>(0x80 | (0x3f & (cp >> 12)));
    result[0] = static_cast<char>(0xF0 | (0x7 & (cp >> 18)));
  }

  return result;
}

enum {
  uintToStringBufferSize = 3 * sizeof(LargestUInt) + 1
};

using UIntToStringBuffer = char[uintToStringBufferSize];

static inline void uintToString(LargestUInt value, char*& current) {
  *--current = 0;
  do {
    *--current = static_cast<char>(value % 10U + static_cast<unsigned>('0'));
    value /= 10;
  } while (value != 0);
}

template <typename Iter> Iter fixNumericLocale(Iter begin, Iter end) {
  for (; begin != end; ++begin) {
    if (*begin == ',') {
      *begin = '.';
    }
  }
  return begin;
}

template <typename Iter> void fixNumericLocaleInput(Iter begin, Iter end) {
  char decimalPoint = getDecimalPoint();
  if (decimalPoint == '\0' || decimalPoint == '.') {
    return;
  }
  for (; begin != end; ++begin) {
    if (*begin == '.') {
      *begin = decimalPoint;
    }
  }
}

template <typename Iter>
Iter fixZerosInTheEnd(Iter begin, Iter end, unsigned int precision) {
  for (; begin != end; --end) {
    if (*(end - 1) != '0') {
      return end;
    }
    if (begin != (end - 1) && begin != (end - 2) && *(end - 2) == '.') {
      if (precision) {
        return end;
      }
      return end - 2;
    }
  }
  return end;
}

} 

#endif 
