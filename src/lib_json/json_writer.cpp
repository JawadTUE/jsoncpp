#if !defined(JSON_IS_AMALGAMATION)
#include "json_tool.h"
#include <json/writer.h>
#endif
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#if __cplusplus >= 201103L
#include <cmath>
#include <cstdio>

#if !defined(isnan)
#define isnan std::isnan
#endif

#if !defined(isfinite)
#define isfinite std::isfinite
#endif

#else
#include <cmath>
#include <cstdio>

#if defined(_MSC_VER)
#if !defined(isnan)
#include <float.h>
#define isnan _isnan
#endif

#if !defined(isfinite)
#include <float.h>
#define isfinite _finite
#endif

#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif

#endif

#if defined(__sun) && defined(__SVR4)
#if !defined(isfinite)
#include <ieeefp.h>
#define isfinite finite
#endif
#endif

#if defined(__hpux)
#if !defined(isfinite)
#if defined(__ia64) && !defined(finite)
#define isfinite(x)                                                            \
  ((sizeof(x) == sizeof(float) ? _Isfinitef(x) : _IsFinite(x)))
#endif
#endif
#endif

#if !defined(isnan)
#define isnan(x) ((x) != (x))
#endif

#if !defined(__APPLE__)
#if !defined(isfinite)
#define isfinite finite
#endif
#endif
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
using StreamWriterPtr = std::unique_ptr<StreamWriter>;
#else
using StreamWriterPtr = std::auto_ptr<StreamWriter>;
#endif

/*!
Converts a signed integer to its string representation, handling negative values and edge cases.
Utilizes a buffer and in-place conversion, adjusting for sign and minimum value scenarios before returning the resulting string.
*/
String valueToString(LargestInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  if (value == Value::minLargestInt) {
    uintToString(LargestUInt(Value::maxLargestInt) + 1, current);
    *--current = '-';
  } else if (value < 0) {
    uintToString(LargestUInt(-value), current);
    *--current = '-';
  } else {
    uintToString(LargestUInt(value), current);
  }
  assert(current >= buffer);
  return current;
}

/*!
Converts an unsigned integer to its string representation using an internal buffer.
Utilizes the `uintToString` function for the actual conversion process and returns the resulting string.
*/
String valueToString(LargestUInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  uintToString(value, current);
  assert(current >= buffer);
  return current;
}

#if defined(JSON_HAS_INT64)

/*!
Converts the given integer value to its string representation by calling an overloaded version of the function that accepts a LargestInt type.
*/
String valueToString(Int value) { return valueToString(LargestInt(value)); }

/*!
Converts the unsigned integer to its string representation by casting it to LargestUInt and calling the corresponding overloaded function.
*/
String valueToString(UInt value) { return valueToString(LargestUInt(value)); }

#endif

namespace {
/*!
\brief Converts a double value to a string representation.

Converts a double value to a string, handling special floating-point values and formatting options.
Supports both significant digits and decimal places precision types.

\param value The double value to convert to a string.
\param useSpecialFloats Flag to determine whether to use special float representations (NaN, Infinity) or alternative forms.
\param precision The number of digits for precision in the output string.
\param precisionType Specifies whether the precision refers to significant digits or decimal places.

\return A string representation of the input double value.
*/
String valueToString(double value, bool useSpecialFloats,
                     unsigned int precision, PrecisionType precisionType) {
  if (!isfinite(value)) {
    static const char* const reps[2][3] = {{"NaN", "-Infinity", "Infinity"},
                                           {"null", "-1e+9999", "1e+9999"}};
    return reps[useSpecialFloats ? 0 : 1][isnan(value)  ? 0
                                          : (value < 0) ? 1
                                                        : 2];
  }

  String buffer(size_t(36), '\0');
  while (true) {
    int len = jsoncpp_snprintf(
        &*buffer.begin(), buffer.size(),
        (precisionType == PrecisionType::significantDigits) ? "%.*g" : "%.*f",
        precision, value);
    assert(len >= 0);
    auto wouldPrint = static_cast<size_t>(len);
    if (wouldPrint >= buffer.size()) {
      buffer.resize(wouldPrint + 1);
      continue;
    }
    buffer.resize(wouldPrint);
    break;
  }

  buffer.erase(fixNumericLocale(buffer.begin(), buffer.end()), buffer.end());

  if (buffer.find('.') == buffer.npos && buffer.find('e') == buffer.npos) {
    buffer += ".0";
  }

  if (precisionType == PrecisionType::decimalPlaces) {
    buffer.erase(fixZerosInTheEnd(buffer.begin(), buffer.end(), precision),
                 buffer.end());
  }

  return buffer;
}
} // namespace

/*!
Delegates the conversion of a double value to a string by calling an overloaded version of the function with additional parameters.
Utilizes default values for precision and precision type if not specified.
*/
String valueToString(double value, unsigned int precision,
                     PrecisionType precisionType) {
  return valueToString(value, false, precision, precisionType);
}

/*!
Converts a boolean value to its string representation, returning "true" for true and "false" for false.
*/
String valueToString(bool value) { return value ? "true" : "false"; }

/*!
\brief Checks if any character in a string requires JSON escaping.

Examines a given string to determine if it contains any characters that need to be escaped when encoding to JSON.
This function is used as an optimization step in JSON string encoding to avoid unnecessary character-by-character processing for strings without special characters.

\param s Pointer to the start of the string to be checked.
\param n Number of characters in the string to check.

\return True if any character requires escaping, false otherwise.
*/
static bool doesAnyCharRequireEscaping(char const* s, size_t n) {
  assert(s || !n);

  return std::any_of(s, s + n, [](unsigned char c) {
    return c == '\\' || c == '"' || c < 0x20 || c > 0x7F;
  });
}

/*!
\brief Converts a UTF-8 encoded character to its Unicode code point.

Parses a UTF-8 encoded character from the input string and converts it to its corresponding Unicode code point.
Handles multi-byte sequences and returns a replacement character for invalid encodings.

\param s Pointer to the current position in the UTF-8 encoded string. Advanced by the number of bytes consumed.
\param e Pointer to the end of the UTF-8 encoded string.

\return The Unicode code point of the parsed character, or a replacement character (U+FFFD) if the encoding is invalid.
*/
static unsigned int utf8ToCodepoint(const char*& s, const char* e) {
  const unsigned int REPLACEMENT_CHARACTER = 0xFFFD;

  unsigned int firstByte = static_cast<unsigned char>(*s);

  if (firstByte < 0x80)
    return firstByte;

  if (firstByte < 0xE0) {
    if (e - s < 2)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated =
        ((firstByte & 0x1F) << 6) | (static_cast<unsigned int>(s[1]) & 0x3F);
    s += 1;
    return calculated < 0x80 ? REPLACEMENT_CHARACTER : calculated;
  }

  if (firstByte < 0xF0) {
    if (e - s < 3)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated = ((firstByte & 0x0F) << 12) |
                              ((static_cast<unsigned int>(s[1]) & 0x3F) << 6) |
                              (static_cast<unsigned int>(s[2]) & 0x3F);
    s += 2;
    if (calculated >= 0xD800 && calculated <= 0xDFFF)
      return REPLACEMENT_CHARACTER;
    return calculated < 0x800 ? REPLACEMENT_CHARACTER : calculated;
  }

  if (firstByte < 0xF8) {
    if (e - s < 4)
      return REPLACEMENT_CHARACTER;

    unsigned int calculated = ((firstByte & 0x07) << 18) |
                              ((static_cast<unsigned int>(s[1]) & 0x3F) << 12) |
                              ((static_cast<unsigned int>(s[2]) & 0x3F) << 6) |
                              (static_cast<unsigned int>(s[3]) & 0x3F);
    s += 3;
    return calculated < 0x10000 ? REPLACEMENT_CHARACTER : calculated;
  }

  return REPLACEMENT_CHARACTER;
}

static const char hex2[] = "000102030405060708090a0b0c0d0e0f"
                           "101112131415161718191a1b1c1d1e1f"
                           "202122232425262728292a2b2c2d2e2f"
                           "303132333435363738393a3b3c3d3e3f"
                           "404142434445464748494a4b4c4d4e4f"
                           "505152535455565758595a5b5c5d5e5f"
                           "606162636465666768696a6b6c6d6e6f"
                           "707172737475767778797a7b7c7d7e7f"
                           "808182838485868788898a8b8c8d8e8f"
                           "909192939495969798999a9b9c9d9e9f"
                           "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"
                           "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"
                           "c0c1c2c3c4c5c6c7c8c9cacbcccdcecf"
                           "d0d1d2d3d4d5d6d7d8d9dadbdcdddedf"
                           "e0e1e2e3e4e5e6e7e8e9eaebecedeeef"
                           "f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff";

/*!
\brief Converts a 16-bit unsigned integer to its hexadecimal representation.

Generates a 4-character string representing the hexadecimal value of a 16-bit unsigned integer.
Used primarily for encoding Unicode characters in JSON strings.

\param x The 16-bit unsigned integer to convert.

\return A 4-character string containing the hexadecimal representation of the input.
*/
static String toHex16Bit(unsigned int x) {
  const unsigned int hi = (x >> 8) & 0xff;
  const unsigned int lo = x & 0xff;
  String result(4, ' ');
  result[0] = hex2[2 * hi];
  result[1] = hex2[2 * hi + 1];
  result[2] = hex2[2 * lo];
  result[3] = hex2[2 * lo + 1];
  return result;
}

/*!
\brief Appends a raw character to a string.

Adds a single character to the end of the given string.
Used in JSON string encoding to append non-escaped characters efficiently, handling both ASCII and Unicode characters.

\param result The string to which the character will be appended.
\param ch The character to append, represented as an unsigned integer.
*/
static void appendRaw(String& result, unsigned ch) {
  result += static_cast<char>(ch);
}

/*!
\brief Appends a Unicode escape sequence to a string.

Converts a character code to its four-digit hexadecimal Unicode representation and appends it to the given string.
Used for escaping special characters and encoding Unicode codepoints in JSON string values.

\param result The string to which the Unicode escape sequence will be appended.
\param ch The character code to be converted and appended as a Unicode escape sequence.
*/
static void appendHex(String& result, unsigned ch) {
  result.append("\\u").append(toHex16Bit(ch));
}

/*!
\brief Converts a string to a properly escaped JSON string representation.

Processes the input string, escaping special characters and handling UTF-8 encoding if necessary.
Wraps the resulting string in double quotes to create a valid JSON string.

\param value The input string to be converted.
\param length The length of the input string.
\param emitUTF8 Flag indicating whether to emit UTF-8 characters directly or escape them.

\return A String containing the properly formatted and escaped JSON string representation.
*/
static String valueToQuotedStringN(const char* value, size_t length,
                                   bool emitUTF8 = false) {
  if (value == nullptr)
    return "";

  if (!doesAnyCharRequireEscaping(value, length))
    return String("\"") + value + "\"";
  String::size_type maxsize = length * 2 + 3;
  String result;
  result.reserve(maxsize);
  result += "\"";
  char const* end = value + length;
  for (const char* c = value; c != end; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    default: {
      if (emitUTF8) {
        unsigned codepoint = static_cast<unsigned char>(*c);
        if (codepoint < 0x20) {
          appendHex(result, codepoint);
        } else {
          appendRaw(result, codepoint);
        }
      } else {
        unsigned codepoint = utf8ToCodepoint(c, end);
        if (codepoint < 0x20) {
          appendHex(result, codepoint);
        } else if (codepoint < 0x80) {
          appendRaw(result, codepoint);
        } else if (codepoint < 0x10000) {
          appendHex(result, codepoint);
        } else {
          codepoint -= 0x10000;
          appendHex(result, 0xd800 + ((codepoint >> 10) & 0x3ff));
          appendHex(result, 0xdc00 + (codepoint & 0x3ff));
        }
      }
    } break;
    }
  }
  result += "\"";
  return result;
}

/*!
Converts a C-string to a JSON-compatible quoted string by delegating to valueToQuotedStringN, using the input's length calculated with strlen.
*/
String valueToQuotedString(const char* value) {
  return valueToQuotedStringN(value, strlen(value));
}

/*!
Delegates the conversion of a character array to a quoted JSON string representation by calling valueToQuotedStringN.
Wraps the result in double quotes to create a valid JSON string.
*/
String valueToQuotedString(const char* value, size_t length) {
  return valueToQuotedStringN(value, length);
}

Writer::~Writer() = default;

FastWriter::FastWriter()

    = default;

/*!
Sets the internal flag to enable YAML compatibility mode, affecting the JSON output format to ensure it is also valid YAML.
*/
void FastWriter::enableYAMLCompatibility() { yamlCompatibilityEnabled_ = true; }

/*!
Sets the internal flag to omit null value placeholders during JSON serialization, resulting in more compact output.
*/
void FastWriter::dropNullPlaceholders() { dropNullPlaceholders_ = true; }

/*!
Sets an internal flag to omit the final line feed character in the JSON output.
This allows for embedding the output in other content without an extra newline at the end.
*/
void FastWriter::omitEndingLineFeed() { omitEndingLineFeed_ = true; }

/*!
Clears the internal document buffer, serializes the input JSON value, and optionally appends a newline character.
Returns the resulting JSON string representation.
*/
String FastWriter::write(const Value& root) {
  document_.clear();
  writeValue(root);
  if (!omitEndingLineFeed_)
    document_ += '\n';
  return document_;
}

/*!
Serializes a JSON value to a string by appending its representation to the internal document buffer.
Handles different value types, including nested structures, and applies formatting based on the writer's configuration.
*/
void FastWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    if (!dropNullPlaceholders_)
      document_ += "null";
    break;
  case intValue:
    document_ += valueToString(value.asLargestInt());
    break;
  case uintValue:
    document_ += valueToString(value.asLargestUInt());
    break;
  case realValue:
    document_ += valueToString(value.asDouble());
    break;
  case stringValue: {
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      document_ += valueToQuotedStringN(str, static_cast<size_t>(end - str));
    break;
  }
  case booleanValue:
    document_ += valueToString(value.asBool());
    break;
  case arrayValue: {
    document_ += '[';
    ArrayIndex size = value.size();
    for (ArrayIndex index = 0; index < size; ++index) {
      if (index > 0)
        document_ += ',';
      writeValue(value[index]);
    }
    document_ += ']';
  } break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    document_ += '{';
    for (auto it = members.begin(); it != members.end(); ++it) {
      const String& name = *it;
      if (it != members.begin())
        document_ += ',';
      document_ += valueToQuotedStringN(name.data(), name.length());
      document_ += yamlCompatibilityEnabled_ ? ": " : ":";
      writeValue(value[name]);
    }
    document_ += '}';
  } break;
  }
}

StyledWriter::StyledWriter() = default;

/*!
Generates a styled string representation of the input JSON value.
Clears the document, writes comments and values, and appends a final newline character to produce a formatted output.
*/
String StyledWriter::write(const Value& root) {
  document_.clear();
  addChildValues_ = false;
  indentString_.clear();
  writeCommentBeforeValue(root);
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  document_ += '\n';
  return document_;
}

/*!
Processes the input JSON value, converting it to its string representation based on its type.
Handles various JSON value types, including nested structures, applying appropriate formatting and indentation for complex types like arrays and objects.
*/
void StyledWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue: {
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(valueToQuotedStringN(str, static_cast<size_t>(end - str)));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        const String& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str(), name.size()));
        document_ += " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

/*!
Formats and writes a JSON array value to the output document.
Handles both single-line and multi-line array representations, applying appropriate indentation, separators, and comments based on the array's content and size.
*/
void StyledWriter::writeArrayValue(const Value& value) {
  size_t size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultilineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      ArrayIndex index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          writeIndent();
          writeValue(childValue);
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        document_ += ',';
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else {
      assert(childValues_.size() == size);
      document_ += "[ ";
      for (size_t index = 0; index < size; ++index) {
        if (index > 0)
          document_ += ", ";
        document_ += childValues_[index];
      }
      document_ += " ]";
    }
  }
}

/*!
Determines if a JSON array should be formatted across multiple lines based on its size, content complexity, and presence of comments.
Analyzes each element, calculates line length, and considers nested arrays or objects to make the decision.
*/
bool StyledWriter::isMultilineArray(const Value& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2;
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

/*!
Appends the given JSON value to either the main document or a temporary storage for child values, depending on the current formatting context.
This allows for flexible handling of inline and multi-line JSON structures.
*/
void StyledWriter::pushValue(const String& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    document_ += value;
}

/*!
Appends appropriate indentation to the JSON document, ensuring proper formatting by adding a newline if needed and avoiding redundant spaces.
Utilizes the current indentation string to maintain consistent formatting throughout the document.
*/
void StyledWriter::writeIndent() {
  if (!document_.empty()) {
    char last = document_[document_.length() - 1];
    if (last == ' ')
      return;
    if (last != '\n')
      document_ += '\n';
  }
  document_ += indentString_;
}

/*!
Applies the current indentation level to the document, then appends the given value.
Ensures proper formatting by adding indentation at the start of new lines in the JSON output.
*/
void StyledWriter::writeWithIndent(const String& value) {
  writeIndent();
  document_ += value;
}

/*!
Appends spaces to the internal indentation string, increasing the current indentation level for formatting nested JSON structures.
*/
void StyledWriter::indent() { indentString_ += String(indentSize_, ' '); }

/*!
Reduces the indentation level by removing one level of indentation from the indentation string.
Ensures proper formatting when closing nested JSON structures.
*/
void StyledWriter::unindent() {
  assert(indentString_.size() >= indentSize_);
  indentString_.resize(indentString_.size() - indentSize_);
}

/*!
Writes the comment preceding a JSON value to the output document.
Handles multi-line comments by preserving line breaks and applying appropriate indentation, ensuring proper formatting of the JSON structure.
*/
void StyledWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  document_ += '\n';
  writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    document_ += *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      writeIndent();
    ++iter;
  }

  document_ += '\n';
}

/*!
Appends comments associated with the JSON value to the document string.
Handles both same-line comments immediately after the value and comments on subsequent lines, preserving the original JSON structure and metadata.
*/
void StyledWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    document_ += " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    document_ += '\n';
    document_ += root.getComment(commentAfter);
    document_ += '\n';
  }
}

/*!
Checks if the given JSON value has any associated comments by examining different comment placements.
Returns true if any type of comment (before, after, or on the same line) is present.
*/
bool StyledWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

/*!
Initializes the writer with a custom indentation string, setting up internal state variables for JSON output formatting.
Moves the provided indentation string into the member variable.
*/
StyledStreamWriter::StyledStreamWriter(String indentation)
    : document_(nullptr), indentation_(std::move(indentation)),
      addChildValues_(), indented_(false) {}

/*!
Writes the JSON value to the output stream with styled formatting.
Initializes writing settings, handles comments before and after the value, applies indentation, and ensures proper formatting of the output.
*/
void StyledStreamWriter::write(OStream& out, const Value& root) {
  document_ = &out;
  addChildValues_ = false;
  indentString_.clear();
  indented_ = true;
  writeCommentBeforeValue(root);
  if (!indented_)
    writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *document_ << "\n";
  document_ = nullptr;
}

/*!
Writes a JSON value to the output stream with appropriate formatting.
Handles all JSON value types, recursively processing complex structures like arrays and objects.
Maintains proper indentation and styling, including handling of comments and member names for objects.
*/
void StyledStreamWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue: {
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(valueToQuotedStringN(str, static_cast<size_t>(end - str)));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        const String& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(valueToQuotedString(name.c_str(), name.size()));
        *document_ << " : ";
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

/*!
Writes a JSON array to the output stream, handling both single-line and multi-line representations.
Manages indentation, spacing, and comments for readable output, recursively processing nested elements and applying appropriate formatting based on array complexity.
*/
void StyledStreamWriter::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultilineArray(value);
    if (isArrayMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_)
            writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *document_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else {
      assert(childValues_.size() == size);
      *document_ << "[ ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *document_ << ", ";
        *document_ << childValues_[index];
      }
      *document_ << " ]";
    }
  }
}

/*!
Determines if an array should be formatted across multiple lines based on its size, content complexity, and formatting constraints.
Analyzes array elements, checks for nested structures and comments, and calculates the potential line length to make the decision.
*/
bool StyledStreamWriter::isMultilineArray(const Value& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2;
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

/*!
Writes the given JSON value to the output stream or stores it in a collection, depending on the current writing context.
This approach allows for flexible handling of JSON elements during the writing process.
*/
void StyledStreamWriter::pushValue(const String& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *document_ << value;
}

/*!
Writes a newline character followed by the current indentation string to the output stream, maintaining proper formatting and readability of the JSON output.
*/
void StyledStreamWriter::writeIndent() { *document_ << '\n' << indentString_; }

/*!
Writes the given value to the output stream, ensuring proper indentation.
If not already indented, it calls writeIndent() before writing the value, then updates the indentation state.
*/
void StyledStreamWriter::writeWithIndent(const String& value) {
  if (!indented_)
    writeIndent();
  *document_ << value;
  indented_ = false;
}

/*!
Increases the current indentation level by appending the indentation string to the existing indentation.
This helps maintain proper hierarchical structure in the formatted JSON output.
*/
void StyledStreamWriter::indent() { indentString_ += indentation_; }

/*!
Decreases the current indentation level by removing the last indentation increment from the indentation string.
Ensures the indentation string is at least as long as the indentation size before resizing.
*/
void StyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

/*!
Writes any comment preceding the JSON value to the output stream, maintaining proper indentation and formatting.
Iterates through the comment characters, handling line breaks and inserting indentation as needed to preserve the JSON structure.
*/
void StyledStreamWriter::writeCommentBeforeValue(const Value& root) {
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_)
    writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *document_ << *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      *document_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

/*!
Writes inline and subsequent comments associated with the JSON value to the output stream.
Handles formatting by adding appropriate spacing and indentation for readability.
*/
void StyledStreamWriter::writeCommentAfterValueOnSameLine(const Value& root) {
  if (root.hasComment(commentAfterOnSameLine))
    *document_ << ' ' << root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *document_ << root.getComment(commentAfter);
  }
  indented_ = false;
}

/*!
Checks if the given JSON value has any associated comments by examining all possible comment placements.
Returns true if any type of comment is present, false otherwise.
*/
bool StyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

/*!
\class CommentStyle
\brief Defines enumeration options for JSON comment handling styles.

Provides a set of enumeration values representing different levels of comment inclusion or handling in JSON operations.
Serves as a configuration option to control how comments are treated during JSON parsing, formatting, or other related tasks.
The available options likely range from ignoring comments to including all comments in the JSON processing.
*/
struct CommentStyle {
  enum Enum { None, Most, All };
};

/*!
\class BuiltStyledStreamWriter
\brief Implements a customizable JSON stream writer with styling options.

Provides a flexible mechanism for writing JSON data to output streams with configurable formatting.
Supports custom indentation, comment styles, symbol representations, and precision settings for floating-point numbers.
Offers methods for writing various JSON value types, managing indentation, and handling comments, enabling the generation of styled and readable JSON output.
*/
struct BuiltStyledStreamWriter : public StreamWriter {
  /*!
  \brief Initializes a BuiltStyledStreamWriter with custom formatting options.
  
  Constructs a BuiltStyledStreamWriter with specified styling parameters.
  Configures the writer's behavior for JSON output formatting, including indentation, comment style, symbol representations, and numeric precision settings.
  
  \param indentation String used for indentation in the output.
  \param cs Enum specifying the comment style to use.
  \param colonSymbol String representation of the colon symbol in key-value pairs.
  \param nullSymbol String representation of null values.
  \param endingLineFeedSymbol String used for line endings.
  \param useSpecialFloats Boolean indicating whether to use special representations for NaN and infinity.
  \param emitUTF8 Boolean determining whether to emit UTF-8 encoded output.
  \param precision Unsigned integer specifying the precision for floating-point number output.
  \param precisionType Enum defining the type of precision to apply to floating-point numbers.
  */
  BuiltStyledStreamWriter(String indentation, CommentStyle::Enum cs,
                          String colonSymbol, String nullSymbol,
                          String endingLineFeedSymbol, bool useSpecialFloats,
                          bool emitUTF8, unsigned int precision,
                          PrecisionType precisionType);
  /*!
  \brief Writes a JSON value to an output stream.
  
  Formats and writes the given JSON value to the specified output stream.
  Applies styling and formatting options according to the writer's configuration, including indentation and comments.
  
  \param root The JSON value to be written to the output stream.
  \param sout Pointer to the output stream where the JSON data will be written.
  
  \return Always returns 0, indicating successful completion of the write operation.
  */
  int write(Value const& root, OStream* sout) override;

private:
  /*!
  \brief Serializes a JSON value to its string representation.
  
  Converts a JSON value to its string representation based on its type.
  Handles null, numeric, string, boolean, array, and object values.
  Applies formatting and styling options as configured in the BuiltStyledStreamWriter.
  
  \param value The JSON value to be serialized.
  */
  void writeValue(Value const& value);
  /*!
  \brief Writes a JSON array value to the output stream.
  
  Formats and writes a JSON array value to the output stream.
  Handles both empty and populated arrays, applying appropriate indentation, separators, and multiline formatting based on the array's content and the configured styling options.
  
  \param value The JSON array value to be written to the output stream.
  */
  void writeArrayValue(Value const& value);
  /*!
  \brief Determines if an array should be written in multiple lines.
  
  Analyzes the given JSON array to decide if it should be formatted across multiple lines.
  Considers the array's size, content complexity, and presence of comments to make this determination.
  This decision affects the readability and structure of the JSON output.
  
  \param value The JSON array value to be analyzed.
  
  \return True if the array should be written in multiple lines, false otherwise.
  */
  bool isMultilineArray(Value const& value);
  /*!
  \brief Writes a JSON value to the output.
  
  Appends the given value either to the internal child values collection or directly to the output stream.
  This method is used as a central mechanism for outputting JSON values, providing flexibility in handling different formatting options and styles.
  
  \param value The JSON value to be written, represented as a string.
  */
  void pushValue(String const& value);
  /*!
  \brief Writes an indentation to the output stream.
  
  Adds a new line followed by the current indentation string to the output stream if indentation is not empty.
  This function is used to maintain proper formatting and readability in the JSON output, especially for nested structures and when adding new elements.
  */
  void writeIndent();
  /*!
  \brief Writes a value with proper indentation.
  
  Ensures proper indentation before writing the given value to the output stream.
  Manages the indentation state to maintain consistent formatting across nested JSON structures.
  
  \param value The string value to be written to the output stream.
  */
  void writeWithIndent(String const& value);
  /*!
  \brief Increases the indentation level.
  
  Adds the current indentation string to the internal indentation buffer.
  This method is used to properly format nested JSON structures, ensuring each level of nesting is visually distinct in the output.
  */
  void indent();
  /*!
  \brief Decreases the current indentation level.
  
  Reduces the indentation string by removing the last indentation unit.
  This function is called after writing nested structures to maintain proper indentation hierarchy in the JSON output.
  */
  void unindent();
  /*!
  \brief Writes a comment before a JSON value.
  
  Handles the output of comments that precede JSON values in the stream.
  Checks if comments are enabled and if the current value has an associated comment.
  If conditions are met, writes the comment with proper indentation and formatting.
  
  \param root The JSON value to check for an associated comment
  */
  void writeCommentBeforeValue(Value const& root);
  /*!
  \brief Writes comments associated with a JSON value.
  
  Handles the output of comments after a JSON value.
  Supports writing comments on the same line as the value and on a new line after the value, based on the configured comment style and the presence of comments in the JSON value.
  
  \param root The JSON value whose associated comments are to be written.
  */
  void writeCommentAfterValueOnSameLine(Value const& root);
  /*!
  \brief Checks if a JSON value has any associated comments.
  
  Determines whether the given JSON value has any comments of type 'before', 'after on same line', or 'after'.
  This information is used in formatting decisions, particularly for array output styling.
  
  \param value The JSON value to check for comments.
  
  \return True if the value has any type of comment, false otherwise.
  */
  static bool hasCommentForValue(const Value& value);

  using ChildValues = std::vector<String>;

  ChildValues childValues_;
  String indentString_;
  unsigned int rightMargin_;
  String indentation_;
  CommentStyle::Enum cs_;
  String colonSymbol_;
  String nullSymbol_;
  String endingLineFeedSymbol_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
  bool useSpecialFloats_ : 1;
  bool emitUTF8_ : 1;
  unsigned int precision_;
  PrecisionType precisionType_;
};
/*!
Initializes the writer with custom formatting options, setting up parameters for indentation, comment style, symbol representations, and numeric precision.
Configures internal state variables to control JSON output styling and formatting behavior.
*/
BuiltStyledStreamWriter::BuiltStyledStreamWriter(
    String indentation, CommentStyle::Enum cs, String colonSymbol,
    String nullSymbol, String endingLineFeedSymbol, bool useSpecialFloats,
    bool emitUTF8, unsigned int precision, PrecisionType precisionType)
    : rightMargin_(74), indentation_(std::move(indentation)), cs_(cs),
      colonSymbol_(std::move(colonSymbol)), nullSymbol_(std::move(nullSymbol)),
      endingLineFeedSymbol_(std::move(endingLineFeedSymbol)),
      addChildValues_(false), indented_(false),
      useSpecialFloats_(useSpecialFloats), emitUTF8_(emitUTF8),
      precision_(precision), precisionType_(precisionType) {}
/*!
Formats and writes the given JSON value to the specified output stream, applying configured styling options.
Manages indentation, comments, and value writing, ensuring proper formatting of the JSON output.
*/
int BuiltStyledStreamWriter::write(Value const& root, OStream* sout) {
  sout_ = sout;
  addChildValues_ = false;
  indented_ = true;
  indentString_.clear();
  writeCommentBeforeValue(root);
  if (!indented_)
    writeIndent();
  indented_ = true;
  writeValue(root);
  writeCommentAfterValueOnSameLine(root);
  *sout_ << endingLineFeedSymbol_;
  sout_ = nullptr;
  return 0;
}
/*!
Writes the JSON value to the output stream based on its type.
Handles various JSON value types, including null, numeric, string, boolean, array, and object, applying appropriate formatting and styling options.
For complex types like arrays and objects, it recursively processes their contents, managing indentation and comments as configured.
*/
void BuiltStyledStreamWriter::writeValue(Value const& value) {
  switch (value.type()) {
  case nullValue:
    pushValue(nullSymbol_);
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble(), useSpecialFloats_, precision_,
                            precisionType_));
    break;
  case stringValue: {
    char const* str;
    char const* end;
    bool ok = value.getString(&str, &end);
    if (ok)
      pushValue(
          valueToQuotedStringN(str, static_cast<size_t>(end - str), emitUTF8_));
    else
      pushValue("");
    break;
  }
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      writeWithIndent("{");
      indent();
      auto it = members.begin();
      for (;;) {
        String const& name = *it;
        Value const& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeWithIndent(
            valueToQuotedStringN(name.data(), name.length(), emitUTF8_));
        *sout_ << colonSymbol_;
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("}");
    }
  } break;
  }
}

/*!
Writes a JSON array to the output stream, handling both single-line and multi-line formats.
Applies appropriate indentation, separators, and styling based on array content and configuration settings.
Recursively processes nested arrays and objects, managing comments and formatting for each element.
*/
void BuiltStyledStreamWriter::writeArrayValue(Value const& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isMultiLine = (cs_ == CommentStyle::All) || isMultilineArray(value);
    if (isMultiLine) {
      writeWithIndent("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        Value const& childValue = value[index];
        writeCommentBeforeValue(childValue);
        if (hasChildValue)
          writeWithIndent(childValues_[index]);
        else {
          if (!indented_)
            writeIndent();
          indented_ = true;
          writeValue(childValue);
          indented_ = false;
        }
        if (++index == size) {
          writeCommentAfterValueOnSameLine(childValue);
          break;
        }
        *sout_ << ",";
        writeCommentAfterValueOnSameLine(childValue);
      }
      unindent();
      writeWithIndent("]");
    } else {
      assert(childValues_.size() == size);
      *sout_ << "[";
      if (!indentation_.empty())
        *sout_ << " ";
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          *sout_ << ((!indentation_.empty()) ? ", " : ",");
        *sout_ << childValues_[index];
      }
      if (!indentation_.empty())
        *sout_ << " ";
      *sout_ << "]";
    }
  }
}

/*!
Analyzes a JSON array to determine if it should be written across multiple lines.
Considers array size, content complexity, and presence of comments, calculating the potential line length and comparing it against the right margin to make the decision.
*/
bool BuiltStyledStreamWriter::isMultilineArray(Value const& value) {
  ArrayIndex const size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (ArrayIndex index = 0; index < size && !isMultiLine; ++index) {
    Value const& childValue = value[index];
    isMultiLine = ((childValue.isArray() || childValue.isObject()) &&
                   !childValue.empty());
  }
  if (!isMultiLine) {
    childValues_.reserve(size);
    addChildValues_ = true;
    ArrayIndex lineLength = 4 + (size - 1) * 2;
    for (ArrayIndex index = 0; index < size; ++index) {
      if (hasCommentForValue(value[index])) {
        isMultiLine = true;
      }
      writeValue(value[index]);
      lineLength += static_cast<ArrayIndex>(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

/*!
Appends the given value to either the internal child values collection or directly to the output stream, depending on the current state.
This method centralizes the output of JSON values, accommodating various formatting options.
*/
void BuiltStyledStreamWriter::pushValue(String const& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    *sout_ << value;
}

/*!
Writes a newline character followed by the current indentation string to the output stream if indentation is not empty.
Ensures proper formatting for nested JSON structures.
*/
void BuiltStyledStreamWriter::writeIndent() {

  if (!indentation_.empty()) {
    *sout_ << '\n' << indentString_;
  }
}

/*!
Writes the given value to the output stream, ensuring proper indentation.
Manages the indentation state by calling writeIndent() if necessary, then outputs the value and updates the indentation flag.
*/
void BuiltStyledStreamWriter::writeWithIndent(String const& value) {
  if (!indented_)
    writeIndent();
  *sout_ << value;
  indented_ = false;
}

/*!
Appends the current indentation string to the internal indentation buffer, effectively increasing the indentation level for nested JSON structures.
*/
void BuiltStyledStreamWriter::indent() { indentString_ += indentation_; }

/*!
Decreases the current indentation level by removing the last indentation unit from the indentation string.
Ensures the indentation string is at least as long as the indentation unit before resizing.
*/
void BuiltStyledStreamWriter::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

/*!
Writes the comment preceding a JSON value to the output stream if comments are enabled and the value has an associated comment.
Handles proper indentation and formatting of multi-line comments, ensuring consistent styling throughout the JSON output.
*/
void BuiltStyledStreamWriter::writeCommentBeforeValue(Value const& root) {
  if (cs_ == CommentStyle::None)
    return;
  if (!root.hasComment(commentBefore))
    return;

  if (!indented_)
    writeIndent();
  const String& comment = root.getComment(commentBefore);
  String::const_iterator iter = comment.begin();
  while (iter != comment.end()) {
    *sout_ << *iter;
    if (*iter == '\n' && ((iter + 1) != comment.end() && *(iter + 1) == '/'))
      *sout_ << indentString_;
    ++iter;
  }
  indented_ = false;
}

/*!
Writes comments associated with a JSON value, handling both same-line and new-line comments based on the configured comment style.
Utilizes the root value's comment data to determine placement and content, writing to the output stream accordingly.
*/
void BuiltStyledStreamWriter::writeCommentAfterValueOnSameLine(
    Value const& root) {
  if (cs_ == CommentStyle::None)
    return;
  if (root.hasComment(commentAfterOnSameLine))
    *sout_ << " " + root.getComment(commentAfterOnSameLine);

  if (root.hasComment(commentAfter)) {
    writeIndent();
    *sout_ << root.getComment(commentAfter);
  }
}

/*!
Checks if the given JSON value has any associated comments by examining three possible comment placements: before, after on the same line, or after.
Returns true if any of these comment types are present.
*/
bool BuiltStyledStreamWriter::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}

/*!
Initializes the StreamWriter object with a null output stream pointer, setting up the base for JSON writing operations.
*/
StreamWriter::StreamWriter() : sout_(nullptr) {}
StreamWriter::~StreamWriter() = default;
StreamWriter::Factory::~Factory() = default;
/*!
Initializes the StreamWriterBuilder with default settings by calling the setDefaults function to populate the internal settings object.
This prepares the builder for creating StreamWriter instances with predefined JSON output formatting configurations.
*/
StreamWriterBuilder::StreamWriterBuilder() { setDefaults(&settings_); }
StreamWriterBuilder::~StreamWriterBuilder() = default;
/*!
Creates a new StreamWriter instance based on the current settings.
Configures various formatting options such as indentation, comment style, precision, and UTF-8 encoding.
Returns a pointer to the newly created StreamWriter object.
*/
StreamWriter* StreamWriterBuilder::newStreamWriter() const {
  const String indentation = settings_["indentation"].asString();
  const String cs_str = settings_["commentStyle"].asString();
  const String pt_str = settings_["precisionType"].asString();
  const bool eyc = settings_["enableYAMLCompatibility"].asBool();
  const bool dnp = settings_["dropNullPlaceholders"].asBool();
  const bool usf = settings_["useSpecialFloats"].asBool();
  const bool emitUTF8 = settings_["emitUTF8"].asBool();
  unsigned int pre = settings_["precision"].asUInt();
  CommentStyle::Enum cs = CommentStyle::All;
  if (cs_str == "All") {
    cs = CommentStyle::All;
  } else if (cs_str == "None") {
    cs = CommentStyle::None;
  } else {
    throwRuntimeError("commentStyle must be 'All' or 'None'");
  }
  PrecisionType precisionType(significantDigits);
  if (pt_str == "significant") {
    precisionType = PrecisionType::significantDigits;
  } else if (pt_str == "decimal") {
    precisionType = PrecisionType::decimalPlaces;
  } else {
    throwRuntimeError("precisionType must be 'significant' or 'decimal'");
  }
  String colonSymbol = " : ";
  if (eyc) {
    colonSymbol = ": ";
  } else if (indentation.empty()) {
    colonSymbol = ":";
  }
  String nullSymbol = "null";
  if (dnp) {
    nullSymbol.clear();
  }
  if (pre > 17)
    pre = 17;
  String endingLineFeedSymbol;
  return new BuiltStyledStreamWriter(indentation, cs, colonSymbol, nullSymbol,
                                     endingLineFeedSymbol, usf, emitUTF8, pre,
                                     precisionType);
}

/*!
Validates the current settings of the StreamWriterBuilder by checking each key against a predefined set of valid keys.
If an invalid key is found, it either returns false immediately or populates the provided Json::Value object with the invalid settings, depending on the input parameter.
*/
bool StreamWriterBuilder::validate(Json::Value* invalid) const {
  static const auto& valid_keys = *new std::set<String>{
      "indentation",
      "commentStyle",
      "enableYAMLCompatibility",
      "dropNullPlaceholders",
      "useSpecialFloats",
      "emitUTF8",
      "precision",
      "precisionType",
  };
  for (auto si = settings_.begin(); si != settings_.end(); ++si) {
    auto key = si.name();
    if (valid_keys.count(key))
      continue;
    if (invalid)
      (*invalid)[key] = *si;
    else
      return false;
  }
  return invalid ? invalid->empty() : true;
}

Value& StreamWriterBuilder::operator[](const String& key) {
  return settings_[key];
}
/*!
Populates the provided Json::Value object with default settings for JSON stream writing.
Sets predefined values for various output configuration options, including comment style, indentation, and numerical precision.
*/
void StreamWriterBuilder::setDefaults(Json::Value* settings) {
  (*settings)["commentStyle"] = "All";
  (*settings)["indentation"] = "\t";
  (*settings)["enableYAMLCompatibility"] = false;
  (*settings)["dropNullPlaceholders"] = false;
  (*settings)["useSpecialFloats"] = false;
  (*settings)["emitUTF8"] = false;
  (*settings)["precision"] = 17;
  (*settings)["precisionType"] = "significant";
}

/*!
Serializes a JSON value to a string using a custom stream writer created by the provided factory.
Writes the JSON value to an output string stream and returns the resulting string.
*/
String writeString(StreamWriter::Factory const& factory, Value const& root) {
  OStringStream sout;
  StreamWriterPtr const writer(factory.newStreamWriter());
  writer->write(root, &sout);
  return std::move(sout).str();
}

OStream& operator<<(OStream& sout, Value const& root) {
  StreamWriterBuilder builder;
  StreamWriterPtr const writer(builder.newStreamWriter());
  writer->write(root, &sout);
  return sout;
}

} // namespace Json
