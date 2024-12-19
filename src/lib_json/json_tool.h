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
/*!
\brief Retrieves the locale-specific decimal point character.

Determines the decimal point character used in the current locale for numeric representation.
This function is used to ensure correct interpretation of numeric values across different localization environments.

\return The locale-specific decimal point character, or '\0' if locale support is disabled or unavailable.
*/
static inline char getDecimalPoint() {
#ifdef JSONCPP_NO_LOCALE_SUPPORT
  return '\0';
#else
  struct lconv* lc = localeconv();
  return lc ? *(lc->decimal_point) : '\0';
#endif
}

/*!
\brief Converts a Unicode code point to its UTF-8 representation.

Encodes a given Unicode code point into its corresponding UTF-8 byte sequence.
Handles code points up to U+10FFFF, covering the entire Unicode range.

\param cp The Unicode code point to be converted.

\return A string containing the UTF-8 encoded representation of the input code point.
*/
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

enum { uintToStringBufferSize = 3 * sizeof(LargestUInt) + 1 };

using UIntToStringBuffer = char[uintToStringBufferSize];

/*!
\brief Converts an unsigned integer to a string representation.

Performs in-place conversion of an unsigned integer to its string representation.
Writes characters from right to left, starting at the position before the current pointer.
Terminates the string with a null character.

\param value The unsigned integer to be converted.
\param current Pointer to the end of a pre-allocated character buffer. Will be decremented as characters are written.
*/
static inline void uintToString(LargestUInt value, char*& current) {
  *--current = 0;
  do {
    *--current = static_cast<char>(value % 10U + static_cast<unsigned>('0'));
    value /= 10;
  } while (value != 0);
}

/*!
\brief Replaces commas with periods in a range of characters.

Ensures consistent decimal notation across different locales by replacing commas with periods.
This function is used to standardize the representation of decimal numbers in JSON output, regardless of the system's locale settings.

\param begin Iterator pointing to the start of the range to be processed.
\param end Iterator pointing to the end of the range to be processed.

\return Iterator pointing to the end of the processed range.
*/
template <typename Iter> Iter fixNumericLocale(Iter begin, Iter end) {
  for (; begin != end; ++begin) {
    if (*begin == ',') {
      *begin = '.';
    }
  }
  return begin;
}

/*!
\brief Adjusts decimal points in a range to match the system locale.

Replaces decimal points ('.') with the locale-specific decimal separator in a given range of characters.
Performs no action if the system decimal separator is already a period or cannot be determined.

\param begin Iterator pointing to the start of the range to be processed.
\param end Iterator pointing to the end of the range to be processed.
*/
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
/*!
\brief Removes trailing zeros from a numeric string representation.

Trims unnecessary trailing zeros after the decimal point in a floating-point number's string representation.
Maintains the specified precision and ensures a clean, concise format for JSON output.

\param begin Iterator pointing to the start of the numeric string.
\param end Iterator pointing to the end of the numeric string.
\param precision The number of decimal places to maintain after trimming.

\return Iterator pointing to the new end of the trimmed numeric string.
*/
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

} // namespace Json

#endif
