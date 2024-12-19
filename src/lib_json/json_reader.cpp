#if !defined(JSON_IS_AMALGAMATION)
#include "json_tool.h"
#include <json/assertions.h>
#include <json/reader.h>
#include <json/value.h>
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <istream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include <cstdio>
#if __cplusplus >= 201103L

#if !defined(sscanf)
#define sscanf std::sscanf
#endif

#endif

#if defined(_MSC_VER)
#if !defined(_CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES)
#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1
#endif
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4996)
#endif

#if !defined(JSONCPP_DEPRECATED_STACK_LIMIT)
#define JSONCPP_DEPRECATED_STACK_LIMIT 1000
#endif

static size_t const stackLimit_g = JSONCPP_DEPRECATED_STACK_LIMIT;

namespace Json {

#if __cplusplus >= 201103L || (defined(_CPPLIB_VER) && _CPPLIB_VER >= 520)
using CharReaderPtr = std::unique_ptr<CharReader>;
#else
using CharReaderPtr = std::auto_ptr<CharReader>;
#endif

Features::Features() = default;

/*!
Creates and returns a Features object with all available JSON parsing and generation options enabled, providing the most flexible configuration for JSON processing.
*/
Features Features::all() { return {}; }

/*!
Creates and returns a Features object configured for strict JSON parsing.
Disables comments, enforces a strict root element, disallows dropped null placeholders, and prohibits numeric keys.
*/
Features Features::strictMode() {
  Features features;
  features.allowComments_ = false;
  features.strictRoot_ = true;
  features.allowDroppedNullPlaceholders_ = false;
  features.allowNumericKeys_ = false;
  return features;
}

/*!
Scans the specified range for newline characters ('\n' or '\r') using the standard algorithm std::any_of.
Returns true if a newline is found, false otherwise.
*/
bool Reader::containsNewLine(Reader::Location begin, Reader::Location end) {
  return std::any_of(begin, end, [](char b) { return b == '\n' || b == '\r'; });
}

/*!
Initializes a new Reader instance with all parsing features enabled by default, setting up the reader for maximum flexibility in JSON interpretation.
*/
Reader::Reader() : features_(Features::all()) {}

/*!
Initializes the Reader with custom parsing features, setting up the behavior for subsequent JSON parsing operations.
*/
Reader::Reader(const Features& features) : features_(features) {}

/*!
Converts the input string to an internal character array and delegates the parsing to the main parse function.
Ensures the entire document is processed by setting the end pointer to the last character.
*/
bool Reader::parse(const std::string& document, Value& root,
                   bool collectComments) {
  document_.assign(document.begin(), document.end());
  const char* begin = document_.c_str();
  const char* end = begin + document_.length();
  return parse(begin, end, root, collectComments);
}

/*!
Reads JSON data from an input stream, converts it to a string, and delegates the parsing to the string-based parse method.
Uses stream iterators for efficient data extraction.
*/
bool Reader::parse(std::istream& is, Value& root, bool collectComments) {

  String doc(std::istreambuf_iterator<char>(is), {});
  return parse(doc.data(), doc.data() + doc.size(), root, collectComments);
}

/*!
Initializes parsing parameters, processes the JSON document, and populates the root Value object.
Handles comment collection, performs strict root validation if enabled, and manages parsing errors.
Returns true if parsing was successful.
*/
bool Reader::parse(const char* beginDoc, const char* endDoc, Value& root,
                   bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = nullptr;
  lastValue_ = nullptr;
  commentsBefore_.clear();
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  bool successful = readValue();
  Token token;
  readTokenSkippingComments(token);
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

/*!
Parses and constructs a JSON value based on the next token in the input stream.
Handles various JSON data types, manages error handling, comment collection, and sets offset information for each parsed value.
*/
bool Reader::readValue() {
  if (nodes_.size() > stackLimit_g)
    throwRuntimeError("Exceeded stackLimit in readValue().");

  Token token;
  readTokenSkippingComments(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_.clear();
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue: {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenFalse: {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNull: {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    }
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValue_ = &currentValue();
  }

  return successful;
}

/*!
Reads the next non-comment token from the input stream.
If comments are allowed, it repeatedly reads tokens until a non-comment token is found or an error occurs.
*/
bool Reader::readTokenSkippingComments(Token& token) {
  bool success = readToken(token);
  if (features_.allowComments_) {
    while (success && token.type_ == tokenComment) {
      success = readToken(token);
    }
  }
  return success;
}

/*!
Reads and identifies the next JSON token from the input stream.
Determines the token type based on the first character encountered, then processes the token accordingly, handling various JSON elements such as delimiters, values, and separators.
*/
bool Reader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
  case '-':
    token.type_ = tokenNumber;
    readNumber();
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return ok;
}

/*!
Advances the current parsing position, skipping over whitespace characters (space, tab, carriage return, newline) until a non-whitespace character is encountered or the end of the input is reached.
*/
void Reader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

/*!
Compares characters at the current parsing position with a given pattern, advancing the position if a match is found.
Ensures the remaining input is long enough before performing character-by-character comparison.
*/
bool Reader::match(const Char* pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

/*!
Identifies and processes C-style or C++-style comments in the JSON input.
If comment collection is enabled, adds the comment to the appropriate location relative to the last parsed value, determining its placement based on the presence of newlines and the comment style.
*/
bool Reader::readComment() {
  Location commentBegin = current_ - 1;
  Char c = getNextChar();
  bool successful = false;
  if (c == '*')
    successful = readCStyleComment();
  else if (c == '/')
    successful = readCppStyleComment();
  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;
    if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
      if (c != '*' || !containsNewLine(commentBegin, current_))
        placement = commentAfterOnSameLine;
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

/*!
Processes the input character range, converting Windows-style line endings (CRLF) to Unix-style (LF).
Creates and returns a new string with normalized line endings, ensuring consistent handling across platforms.
*/
String Reader::normalizeEOL(Reader::Location begin, Reader::Location end) {
  String normalized;
  normalized.reserve(static_cast<size_t>(end - begin));
  Reader::Location current = begin;
  while (current != end) {
    char c = *current++;
    if (c == '\r') {
      if (current != end && *current == '\n')
        ++current;
      normalized += '\n';
    } else {
      normalized += c;
    }
  }
  return normalized;
}

/*!
Normalizes and adds a comment to the JSON structure, either attaching it to the last parsed value or storing it for later use.
The comment's placement determines how it's handled, with normalization applied to ensure consistent line endings.
*/
void Reader::addComment(Location begin, Location end,
                        CommentPlacement placement) {
  assert(collectComments_);
  const String& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != nullptr);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

/*!
Consumes characters from the input stream until the end of a C-style comment is encountered.
Returns true if the comment was successfully consumed, advancing the current position past the closing '* /'.
*/
bool Reader::readCStyleComment() {
  while ((current_ + 1) < end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
  }
  return getNextChar() == '/';
}

/*!
Reads and skips characters until the end of the current line, handling both '\n' and '\r\n' line endings.
Utilizes getNextChar() to advance through the input stream, effectively ignoring C++-style comments.
*/
bool Reader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      break;
    }
  }
  return true;
}

/*!
Parses a numeric token from the input stream, advancing the current position.
Handles integers, floating-point numbers, and numbers with exponents without performing actual conversion.
*/
void Reader::readNumber() {
  Location p = current_;
  char c = '0';
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : '\0';
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
}

/*!
Processes characters in the input stream until encountering the closing double quote of a JSON string.
Handles escape characters and advances the current position using getNextChar().
*/
bool Reader::readString() {
  Char c = '\0';
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}

/*!
Parses a JSON object by reading key-value pairs from the input stream.
Handles string and numeric keys, nested structures, and maintains error recovery mechanisms.
Updates the current value with the parsed object data, setting appropriate offsets and managing the parsing state.
*/
bool Reader::readObject(Token& token) {
  Token tokenName;
  String name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  while (readTokenSkippingComments(tokenName)) {
    if (tokenName.type_ == tokenObjectEnd && name.empty())
      return true;
    name.clear();
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover("Missing ':' after object member name", colon,
                                tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok)
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readTokenSkippingComments(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator)) {
      return addErrorAndRecover("Missing ',' or '}' in object declaration",
                                comma, tokenObjectEnd);
    }
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover("Missing '}' or object member name", tokenName,
                            tokenObjectEnd);
}

/*!
Parses a JSON array, populating the current value with array elements.
Handles empty arrays, nested structures, and array separators.
Continues parsing until the closing bracket is encountered or an error occurs.
*/
bool Reader::readArray(Token& token) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  skipSpaces();
  if (current_ != end_ && *current_ == ']') {
    Token endArray;
    readToken(endArray);
    return true;
  }
  int index = 0;
  for (;;) {
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok)
      return recoverFromError(tokenArrayEnd);

    Token currentToken;
    ok = readTokenSkippingComments(currentToken);
    bool badTokenType = (currentToken.type_ != tokenArraySeparator &&
                         currentToken.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover("Missing ',' or ']' in array declaration",
                                currentToken, tokenArrayEnd);
    }
    if (currentToken.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

/*!
Decodes a numeric token into a JSON value, updating the current value with the decoded result.
Sets the offset information for the parsed number based on its position in the input stream.
*/
bool Reader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Parses a numeric token, attempting to represent it as an integer if possible, or falling back to floating-point representation.
Handles negative numbers and checks for overflow conditions, delegating to decodeDouble for non-integer or large values.
*/
bool Reader::decodeNumber(Token& token, Value& decoded) {
  Location current = token.start_;
  bool isNegative = *current == '-';
  if (isNegative)
    ++current;
  Value::LargestUInt maxIntegerValue =
      isNegative ? Value::LargestUInt(Value::maxLargestInt) + 1
                 : Value::maxLargestUInt;
  Value::LargestUInt threshold = maxIntegerValue / 10;
  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);
    auto digit(static_cast<Value::UInt>(c - '0'));
    if (value >= threshold) {
      if (value > threshold || current != token.end_ ||
          digit > maxIntegerValue % 10) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }
  if (isNegative && value == maxIntegerValue)
    decoded = Value::minLargestInt;
  else if (isNegative)
    decoded = -Value::LargestInt(value);
  else if (value <= Value::LargestUInt(Value::maxInt))
    decoded = Value::LargestInt(value);
  else
    decoded = value;
  return true;
}

/*!
Decodes a double-precision floating-point number from the given token and updates the current JSON value.
Sets the offset start and limit for the decoded value based on the token's position in the input stream.
*/
bool Reader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Attempts to parse a double value from the given token, handling special cases like infinity.
Stores the parsed value in the decoded parameter if successful, or adds an error if parsing fails.
*/
bool Reader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  IStringStream is(String(token.start_, token.end_));
  if (!(is >> value)) {
    if (value == std::numeric_limits<double>::max())
      value = std::numeric_limits<double>::infinity();
    else if (value == std::numeric_limits<double>::lowest())
      value = -std::numeric_limits<double>::infinity();
    else if (!std::isinf(value))
      return addError(
          "'" + String(token.start_, token.end_) + "' is not a number.", token);
  }
  decoded = value;
  return true;
}

/*!
Decodes the JSON string token, updates the current Value object with the decoded string, and sets its offset information.
Returns true if successful, false otherwise.
*/
bool Reader::decodeString(Token& token) {
  String decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Processes a JSON string token, handling escape sequences and Unicode code points.
Iterates through the token, decoding special characters and escape sequences, and stores the result in the provided string.
*/
bool Reader::decodeString(Token& token, String& decoded) {
  decoded.reserve(static_cast<size_t>(token.end_ - token.start_ - 2));
  Location current = token.start_ + 1;
  Location end = token.end_ - 1;
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

/*!
Decodes a Unicode code point from a JSON string, handling surrogate pairs for characters outside the Basic Multilingual Plane.
Utilizes helper functions to parse escape sequences and manage errors, advancing the current position in the string as it processes.
*/
bool Reader::decodeUnicodeCodePoint(Token& token, Location& current,
                                    Location end, unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token, current);
    if (*(current++) == '\\' && *(current++) == 'u') {
      unsigned int surrogatePair;
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token, current);
  }
  return true;
}

/*!
Decodes a four-digit hexadecimal Unicode escape sequence in the input stream, converting it to its corresponding integer value.
Validates the sequence length and character validity, updating the current position and storing the result in the output parameter.
*/
bool Reader::decodeUnicodeEscapeSequence(Token& token, Location& current,
                                         Location end,
                                         unsigned int& ret_unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.", token,
        current);
  int unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token, current);
  }
  ret_unicode = static_cast<unsigned int>(unicode);
  return true;
}

/*!
Adds an error to the internal error collection, storing the provided message, token, and location information.
Returns false to indicate an error has occurred.
*/
bool Reader::addError(const String& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

/*!
Skips tokens in the input stream until a specified token type or end of stream is reached, resetting the error count to its initial state.
Always returns false to indicate an error occurred and recovery was attempted.
*/
bool Reader::recoverFromError(TokenType skipUntilToken) {
  size_t const errorCount = errors_.size();
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount);
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

/*!
Adds an error message to the internal collection and attempts to recover from the parsing error by skipping tokens until a specified token type is reached.
Returns true if recovery was successful, false otherwise.
*/
bool Reader::addErrorAndRecover(const String& message, Token& token,
                                TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

/*!
Returns a reference to the top element of the nodes stack, representing the current JSON value being processed.
This allows efficient access and manipulation of the JSON structure during parsing.
*/
Value& Reader::currentValue() { return *(nodes_.top()); }

/*!
Retrieves and returns the next character from the input stream, advancing the current position.
Returns 0 if the end of the stream is reached.
*/
Reader::Char Reader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

/*!
Calculates line and column numbers for a given location in the JSON input by iterating through the input, tracking line breaks and counting characters.
Updates the provided line and column references with the calculated values.
*/
void Reader::getLocationLineAndColumn(Location location, int& line,
                                      int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (current != end_ && *current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  column = int(location - lastLineStart) + 1;
  ++line;
}

/*!
Converts the given location to a formatted string representation of line and column numbers.
Utilizes an internal method to calculate the precise position and formats the result into a human-readable string.
*/
String Reader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  jsoncpp_snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

String Reader::getFormatedErrorMessages() const {
  return getFormattedErrorMessages();
}

/*!
Constructs a formatted error message string by iterating through collected parsing errors.
For each error, it includes the error location, message, and any additional details, concatenating them into a single string.
*/
String Reader::getFormattedErrorMessages() const {
  String formattedMessage;
  for (const auto& error : errors_) {
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

/*!
Converts internal error data into a vector of StructuredError objects, containing start and end offsets and error messages for each parsing error.
Returns the vector of structured errors.
*/
std::vector<Reader::StructuredError> Reader::getStructuredErrors() const {
  std::vector<Reader::StructuredError> allErrors;
  for (const auto& error : errors_) {
    Reader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

/*!
Adds an error to the internal error list with location information derived from the provided Value object.
Validates the offset information, creates an error token, and appends the error details to the errors_ vector if successful.
*/
bool Reader::pushError(const Value& value, const String& message) {
  ptrdiff_t const length = end_ - begin_;
  if (value.getOffsetStart() > length || value.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = nullptr;
  errors_.push_back(info);
  return true;
}

/*!
Creates and adds an error token to the internal error list based on the provided JSON value, message, and extra context.
Validates offset information before adding the error, ensuring accurate error reporting within the parsed JSON structure.
*/
bool Reader::pushError(const Value& value, const String& message,
                       const Value& extra) {
  ptrdiff_t const length = end_ - begin_;
  if (value.getOffsetStart() > length || value.getOffsetLimit() > length ||
      extra.getOffsetLimit() > length)
    return false;
  Token token;
  token.type_ = tokenError;
  token.start_ = begin_ + value.getOffsetStart();
  token.end_ = begin_ + value.getOffsetLimit();
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = begin_ + extra.getOffsetStart();
  errors_.push_back(info);
  return true;
}

/*!
Returns true if no parsing errors occurred, by checking if the errors collection is empty.
*/
bool Reader::good() const { return errors_.empty(); }

/*!
\class OurFeatures
\brief Configures JSON parsing and serialization options.

Provides a set of boolean flags and settings to customize JSON processing behavior.
Allows users to control various aspects of parsing, such as handling comments, trailing commas, and special floating-point values.
Includes a stack limit setting and offers a static method to create an instance with default configurations.
Enables fine-tuning of JSON parsing and serialization according to specific project requirements.
*/
class OurFeatures {
public:
  /*!
  \brief Creates a default set of parsing features.
  
  Initializes and returns an OurFeatures object with default settings for JSON parsing.
  This function serves as a starting point for configuring the parsing behavior, which can be further customized as needed.
  
  \return An OurFeatures object with default parsing settings.
  */
  static OurFeatures all();
  bool allowComments_;
  bool allowTrailingCommas_;
  bool strictRoot_;
  bool allowDroppedNullPlaceholders_;
  bool allowNumericKeys_;
  bool allowSingleQuotes_;
  bool failIfExtra_;
  bool rejectDupKeys_;
  bool allowSpecialFloats_;
  bool skipBom_;
  size_t stackLimit_;
};

/*!
Creates and returns a default OurFeatures object with all parsing options set to their initial values.
This provides a starting point for customizing JSON parsing behavior.
*/
OurFeatures OurFeatures::all() { return {}; }

/*!
\class OurReader
\brief Parses and processes JSON documents.

Provides functionality for reading and interpreting JSON data.
Offers methods for parsing JSON strings, handling various JSON elements, managing tokens, and reporting errors.
Supports features such as comment handling, error recovery, and customizable parsing options.
Designed for efficient and flexible JSON document processing.
*/
class OurReader {
public:
  using Char = char;
  using Location = const Char*;

  /*!
  \brief Constructs a JSON reader with specified features.
  
  Initializes a new OurReader instance with the provided feature set.
  This constructor allows customization of the reader's behavior through the features parameter, enabling fine-tuned control over parsing options.
  
  \param features Configuration options that define the behavior of the JSON reader.
  */
  explicit OurReader(OurFeatures const& features);
  /*!
  \brief Parses a JSON document.
  
  Processes the input JSON document, populating the root value with the parsed data.
  Handles comments, strict root requirements, and extra content based on specified features.
  Reports parsing errors if encountered.
  
  \param beginDoc Pointer to the beginning of the JSON document.
  \param endDoc Pointer to the end of the JSON document.
  \param root Value object to store the parsed JSON data.
  \param collectComments Flag indicating whether to collect comments during parsing.
  
  \return True if parsing was successful, false otherwise.
  */
  bool parse(const char* beginDoc, const char* endDoc, Value& root,
             bool collectComments = true);
  /*!
  \brief Retrieves formatted error messages from the parsing process.
  
  Compiles all parsing errors encountered during JSON processing into a single, formatted string.
  Each error message includes the location (line and column) of the error, the error message itself, and additional details if available.
  
  \return A string containing all formatted error messages, with each error on separate lines and including location information.
  */
  String getFormattedErrorMessages() const;
  /*!
  \brief Retrieves structured error information from the JSON parsing process.
  
  Converts internal error representations into a standardized format.
  Creates a vector of StructuredError objects, each containing the start and end offsets of the erroneous token within the input stream, along with an associated error message.
  
  \return A vector of CharReader::StructuredError objects representing all parsing errors encountered.
  */
  std::vector<CharReader::StructuredError> getStructuredErrors() const;

private:
  OurReader(OurReader const&);
  void operator=(OurReader const&);

  enum TokenType {
    tokenEndOfStream = 0,
    tokenObjectBegin,
    tokenObjectEnd,
    tokenArrayBegin,
    tokenArrayEnd,
    tokenString,
    tokenNumber,
    tokenTrue,
    tokenFalse,
    tokenNull,
    tokenNaN,
    tokenPosInf,
    tokenNegInf,
    tokenArraySeparator,
    tokenMemberSeparator,
    tokenComment,
    tokenError
  };

  /*!
  \class Token
  \brief Represents a single token in the JSON parsing process.
  
  Stores information about a token's type and its location within the source text.
  Used within the OurReader component to facilitate JSON parsing by providing a simple data structure for token representation.
  Contains public members for token type and position markers.
  */
  class Token {
  public:
    TokenType type_;
    Location start_;
    Location end_;
  };

  /*!
  \class ErrorInfo
  \brief Stores information about errors encountered during JSON parsing.
  
  Contains details about parsing errors, including the token where the error occurred, an error message, and location information.
  Used within the OurReader class to provide comprehensive error reporting during JSON document processing.
  */
  class ErrorInfo {
  public:
    Token token_;
    String message_;
    Location extra_;
  };

  using Errors = std::deque<ErrorInfo>;

  /*!
  \brief Reads and identifies the next token in the JSON input stream.
  
  Examines the current character in the input stream and determines the appropriate token type.
  Handles various JSON elements including delimiters, strings, numbers, boolean values, null, and special floating-point values.
  Also processes comments and separators.
  
  \param token The Token object to be filled with the identified token's information, including its type and position in the stream.
  
  \return True if a valid token was successfully read, false otherwise.
  */
  bool readToken(Token& token);
  /*!
  \brief Reads a token while skipping comments.
  
  Reads tokens from the input, skipping any comments if they are allowed by the parser's features.
  Continues reading until a non-comment token is found or the end of input is reached.
  
  \param token The Token object to be filled with the read token's information.
  
  \return True if a token was successfully read, false if the end of input was reached or an error occurred.
  */
  bool readTokenSkippingComments(Token& token);
  /*!
  \brief Skips whitespace characters in the input stream.
  
  Advances the current position in the input stream until a non-whitespace character is encountered.
  Whitespace characters include spaces, tabs, carriage returns, and newlines.
  This function is used to prepare for reading meaningful JSON tokens by ignoring insignificant whitespace between structural elements.
  */
  void skipSpaces();
  /*!
  \brief Handles the Byte Order Mark at the beginning of a JSON document.
  
  Detects and skips the UTF-8 Byte Order Mark (BOM) if present at the beginning of the input.
  This ensures correct parsing of JSON documents that may include a BOM, enhancing compatibility with various file encodings.
  
  \param skipBom Flag indicating whether to skip the BOM. If true, the function will check for and skip the BOM if present.
  */
  void skipBom(bool skipBom);
  /*!
  \brief Matches a pattern in the input stream.
  
  Compares the current position in the input stream with a given pattern.
  Advances the current position if the pattern matches.
  This function is crucial for identifying specific tokens during JSON parsing.
  
  \param pattern The pattern to match against the current input stream position.
  \param patternLength The length of the pattern to match.
  
  \return True if the pattern matches, false otherwise.
  */
  bool match(const Char* pattern, int patternLength);
  /*!
  \brief Reads and processes a comment in the JSON input.
  
  Identifies and handles both C-style and C++-style comments in the JSON input.
  Determines the comment placement relative to the last parsed value and optionally collects comments if enabled.
  Supports proper JSON parsing by allowing the parser to handle and potentially preserve comments within the JSON structure.
  
  \return True if a comment was successfully read and processed, false otherwise.
  */
  bool readComment();
  /*!
  \brief Reads a C-style comment.
  
  Parses a C-style comment in the JSON input.
  Advances the current position to the end of the comment and checks for newline characters within the comment.
  
  \param containsNewLineResult Pointer to a boolean that will be set to true if the comment contains a newline character, false otherwise.
  
  \return True if the comment was successfully parsed and terminated, false otherwise.
  */
  bool readCStyleComment(bool* containsNewLineResult);
  /*!
  \brief Skips a C++-style comment.
  
  Reads and discards characters until the end of the line, handling both Unix ('\n') and Windows ('\r\n') style line endings.
  This function is used to ignore C++-style comments during JSON parsing.
  
  \return Always true, indicating successful comment processing.
  */
  bool readCppStyleComment();
  /*!
  \brief Reads a JSON string token.
  
  Processes characters in the input stream until the closing double quote of a string is encountered.
  Handles escape sequences within the string but does not interpret them.
  
  \return True if the string was successfully read (closing quote found), false otherwise.
  */
  bool readString();
  /*!
  \brief Reads a single-quoted string token.
  
  Processes characters within a single-quoted string, handling escape sequences.
  Continues reading until the closing single quote is encountered or the end of input is reached.
  
  \return True if the string was properly terminated with a single quote, false otherwise.
  */
  bool readStringSingleQuote();
  /*!
  \brief Parses a JSON number from the input stream.
  
  Reads and validates a numerical value from the current position in the JSON input.
  Handles integer and floating-point numbers, including those with exponents.
  Advances the current position to the end of the number if successful.
  
  \param checkInf If true, checks for the 'Infinity' literal at the current position. If false, skips this check.
  
  \return True if a valid number was read, false if 'Infinity' was detected when checkInf is true.
  */
  bool readNumber(bool checkInf);
  /*!
  \brief Parses and constructs a JSON value.
  
  Reads the next token from the input stream and constructs the corresponding JSON value.
  Handles various value types including objects, arrays, numbers, strings, booleans, null, and special numeric values.
  Manages comments and enforces stack limits during parsing.
  
  \return True if the value was successfully parsed, false otherwise.
  */
  bool readValue();
  /*!
  \brief Parses a JSON object.
  
  Reads and processes key-value pairs within a JSON object.
  Handles various features such as trailing commas, numeric keys, and duplicate key rejection.
  Populates the current Value object with parsed data.
  
  \param token The token representing the start of the JSON object.
  
  \return True if the object was successfully parsed, false otherwise.
  */
  bool readObject(Token& token);
  /*!
  \brief Parses a JSON array.
  
  Reads and processes the contents of a JSON array.
  Iterates through array elements, handling nested values, commas, and the closing bracket.
  Supports features like trailing commas based on parser configuration.
  
  \param token The token representing the start of the array.
  
  \return True if the array was successfully parsed, false otherwise.
  */
  bool readArray(Token& token);
  /*!
  \brief Decodes a numeric token into a JSON value.
  
  Parses the given token as a number and stores it in the current JSON value.
  Updates the offset information for the parsed number in the current value.
  
  \param token The token containing the numeric string to be decoded.
  
  \return true if the number was successfully decoded, false otherwise.
  */
  bool decodeNumber(Token& token);
  /*!
  \brief Decodes a JSON number token into a Value object.
  
  Parses a JSON number token and converts it into an integer or double value.
  Handles both positive and negative numbers, and switches to double parsing if the number exceeds integer limits.
  
  \param token The token containing the JSON number to be decoded.
  \param decoded The Value object where the decoded number will be stored.
  
  \return True if the number was successfully decoded, false if decoding failed.
  */
  bool decodeNumber(Token& token, Value& decoded);
  /*!
  \brief Decodes a JSON string token.
  
  Decodes the given token as a JSON string, handling any necessary character escaping or Unicode conversion.
  Sets the decoded string as the current value in the JSON structure being built.
  
  \param token The token representing the JSON string to be decoded.
  
  \return True if the string was successfully decoded, false otherwise.
  */
  bool decodeString(Token& token);
  /*!
  \brief Decodes a JSON string token.
  
  Processes a JSON string token, handling escape sequences and Unicode code points.
  Converts the encoded string into its decoded form, managing various escape characters and UTF-8 encoding.
  
  \param token The input token containing the JSON string to be decoded.
  \param decoded The output string where the decoded result is stored.
  
  \return True if the string was successfully decoded, false if an error occurred during decoding.
  */
  bool decodeString(Token& token, String& decoded);
  /*!
  \brief Decodes a double-precision floating-point number from a token.
  
  Parses and converts a complex numerical representation into a double-precision floating-point value.
  This function is used as a fallback when decodeNumber encounters numbers that cannot be accurately represented as integers, such as very large numbers, decimals, or numbers in scientific notation.
  
  \param token The token containing the numerical string to be decoded.
  
  \return true if the decoding was successful, false otherwise.
  */
  bool decodeDouble(Token& token);
  /*!
  \brief Decodes a double value from a token.
  
  Attempts to parse a double value from the given token.
  Handles special cases such as infinity and error conditions.
  Sets the decoded value if successful.
  
  \param token The token containing the string representation of the double value to be decoded.
  \param decoded The Value object where the decoded double will be stored if successful.
  
  \return True if the decoding was successful, false otherwise.
  */
  bool decodeDouble(Token& token, Value& decoded);
  /*!
  \brief Decodes a Unicode code point from a JSON string.
  
  Processes Unicode escape sequences in JSON strings, handling both single code points and surrogate pairs.
  Ensures proper representation of international characters and symbols within JSON data.
  
  \param token The token being processed
  \param current Current position in the JSON string
  \param end End position of the JSON string
  \param unicode The decoded Unicode code point
  
  \return True if decoding was successful, false otherwise
  */
  bool decodeUnicodeCodePoint(Token& token, Location& current, Location end,
                              unsigned int& unicode);
  /*!
  \brief Decodes a Unicode escape sequence.
  
  Parses a four-digit hexadecimal Unicode escape sequence and converts it to its corresponding integer value.
  This function is used within the JSON parsing process to handle escaped Unicode characters in strings.
  
  \param token The token being processed, used for error reporting.
  \param current The current position in the input stream.
  \param end The end position of the input stream.
  \param unicode The output parameter where the decoded Unicode value is stored.
  
  \return True if the decoding was successful, false otherwise.
  */
  bool decodeUnicodeEscapeSequence(Token& token, Location& current,
                                   Location end, unsigned int& unicode);
  /*!
  \brief Adds an error to the internal error collection.
  
  Creates an ErrorInfo object with the provided message, token, and extra location.
  Appends this error information to the internal errors collection for later reporting or processing.
  
  \param message The error message describing the issue encountered during parsing.
  \param token The token associated with the error, typically where the error was detected.
  \param extra Additional location information related to the error, if applicable.
  
  \return Always false, indicating an error condition to the caller.
  */
  bool addError(const String& message, Token& token, Location extra = nullptr);
  /*!
  \brief Attempts to recover from a parsing error.
  
  Skips tokens in the input stream until a specified token type or the end of the stream is reached.
  This function is used to continue parsing after encountering an error, allowing for more comprehensive error reporting and increased parser robustness.
  
  \param skipUntilToken The token type to search for when skipping. Parsing will resume when this token type is found or the end of the stream is reached.
  
  \return Always false, indicating that an error occurred and recovery was attempted.
  */
  bool recoverFromError(TokenType skipUntilToken);
  /*!
  \brief Adds an error and attempts to recover from it.
  
  Reports an error associated with a specific token and attempts to recover by skipping tokens until a specified token type is reached.
  This allows parsing to continue after encountering errors in JSON structures.
  
  \param message The error message to be added.
  \param token The token associated with the error.
  \param skipUntilToken The token type to skip to during error recovery.
  
  \return True if recovery was successful, false otherwise.
  */
  bool addErrorAndRecover(const String& message, Token& token,
                          TokenType skipUntilToken);
  void skipUntilSpace();
  /*!
  \brief Retrieves the current JSON value being processed.
  
  Provides access to the top element of the nodes stack, representing the current JSON value during parsing.
  This function is essential for manipulating and constructing the JSON object hierarchy as the parser processes input data.
  
  \return A reference to the current JSON Value object at the top of the nodes stack.
  */
  Value& currentValue();
  /*!
  \brief Retrieves the next character from the input stream.
  
  Advances the current position in the input stream and returns the character at that position.
  This function is essential for iterating through the JSON input during parsing operations.
  
  \return The next character in the input stream, or 0 if the end of the stream has been reached.
  */
  Char getNextChar();
  /*!
  \brief Calculates line and column numbers for a given location in the JSON document.
  
  Determines the line and column numbers corresponding to a specific location within the JSON input.
  This function is used for error reporting, providing precise position information for parsing errors.
  
  \param location The position in the JSON document for which to calculate line and column numbers.
  \param line The calculated line number, which will be set by this function.
  \param column The calculated column number, which will be set by this function.
  */
  void getLocationLineAndColumn(Location location, int& line,
                                int& column) const;
  /*!
  \brief Converts a location to a string representation.
  
  Generates a human-readable string describing the line and column of a given location in the JSON document.
  This is useful for error reporting and debugging.
  
  \param location The location in the JSON document to be converted.
  
  \return A string containing the line and column information of the given location.
  */
  String getLocationLineAndColumn(Location location) const;
  /*!
  \brief Adds a comment to the JSON structure.
  
  Processes and stores a comment encountered during JSON parsing.
  Normalizes the comment's end-of-line characters and either associates it with the last parsed value or adds it to a collection of comments for the next value, based on the specified placement.
  
  \param begin The starting location of the comment in the input stream.
  \param end The ending location of the comment in the input stream.
  \param placement Specifies where the comment should be placed in relation to JSON values.
  */
  void addComment(Location begin, Location end, CommentPlacement placement);

  /*!
  \brief Normalizes line endings in a string.
  
  Converts different line ending formats (CR, LF, or CRLF) to a consistent LF format.
  This function is primarily used for standardizing line endings in comments within JSON data, ensuring uniform storage across different platforms and input sources.
  
  \param begin Iterator pointing to the start of the input string.
  \param end Iterator pointing to the end of the input string.
  
  \return A new string with normalized line endings (LF only).
  */
  static String normalizeEOL(Location begin, Location end);
  /*!
  \brief Checks for newline characters in a given range.
  
  Scans the range between two locations for the presence of newline characters ('\n' or '\r').
  Used to determine appropriate comment placement in JSON structures.
  
  \param begin The starting location of the range to check.
  \param end The ending location of the range to check.
  
  \return True if a newline character is found within the range, false otherwise.
  */
  static bool containsNewLine(Location begin, Location end);

  using Nodes = std::stack<Value*>;

  Nodes nodes_{};
  Errors errors_{};
  String document_{};
  Location begin_ = nullptr;
  Location end_ = nullptr;
  Location current_ = nullptr;
  Location lastValueEnd_ = nullptr;
  Value* lastValue_ = nullptr;
  bool lastValueHasAComment_ = false;
  String commentsBefore_{};

  OurFeatures const features_;
  bool collectComments_ = false;
};

/*!
Scans the given range for newline characters ('\n' or '\r') using the standard algorithm std::any_of.
Returns true if a newline is found, false otherwise.
*/
bool OurReader::containsNewLine(OurReader::Location begin,
                                OurReader::Location end) {
  return std::any_of(begin, end, [](char b) { return b == '\n' || b == '\r'; });
}

/*!
Initializes the reader with the specified features, setting up the parsing behavior according to the provided configuration options.
*/
OurReader::OurReader(OurFeatures const& features) : features_(features) {}

/*!
Parses a JSON document, populating the root value with parsed data.
Handles comments, enforces strict root requirements if enabled, and processes the input while managing parsing state and error collection.
Returns true if parsing was successful.
*/
bool OurReader::parse(const char* beginDoc, const char* endDoc, Value& root,
                      bool collectComments) {
  if (!features_.allowComments_) {
    collectComments = false;
  }

  begin_ = beginDoc;
  end_ = endDoc;
  collectComments_ = collectComments;
  current_ = begin_;
  lastValueEnd_ = nullptr;
  lastValue_ = nullptr;
  commentsBefore_.clear();
  errors_.clear();
  while (!nodes_.empty())
    nodes_.pop();
  nodes_.push(&root);

  skipBom(features_.skipBom_);
  bool successful = readValue();
  nodes_.pop();
  Token token;
  readTokenSkippingComments(token);
  if (features_.failIfExtra_ && (token.type_ != tokenEndOfStream)) {
    addError("Extra non-whitespace after JSON value.", token);
    return false;
  }
  if (collectComments_ && !commentsBefore_.empty())
    root.setComment(commentsBefore_, commentAfter);
  if (features_.strictRoot_) {
    if (!root.isArray() && !root.isObject()) {
      token.type_ = tokenError;
      token.start_ = beginDoc;
      token.end_ = endDoc;
      addError(
          "A valid JSON document must be either an array or an object value.",
          token);
      return false;
    }
  }
  return successful;
}

/*!
Parses and constructs a JSON value based on the next token from the input stream.
Handles various value types, manages comments, and sets offset information for the parsed value.
Returns true if parsing is successful, false otherwise.
*/
bool OurReader::readValue() {
  if (nodes_.size() > features_.stackLimit_)
    throwRuntimeError("Exceeded stackLimit in readValue().");
  Token token;
  readTokenSkippingComments(token);
  bool successful = true;

  if (collectComments_ && !commentsBefore_.empty()) {
    currentValue().setComment(commentsBefore_, commentBefore);
    commentsBefore_.clear();
  }

  switch (token.type_) {
  case tokenObjectBegin:
    successful = readObject(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenArrayBegin:
    successful = readArray(token);
    currentValue().setOffsetLimit(current_ - begin_);
    break;
  case tokenNumber:
    successful = decodeNumber(token);
    break;
  case tokenString:
    successful = decodeString(token);
    break;
  case tokenTrue: {
    Value v(true);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenFalse: {
    Value v(false);
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNull: {
    Value v;
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNaN: {
    Value v(std::numeric_limits<double>::quiet_NaN());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenPosInf: {
    Value v(std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenNegInf: {
    Value v(-std::numeric_limits<double>::infinity());
    currentValue().swapPayload(v);
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
  } break;
  case tokenArraySeparator:
  case tokenObjectEnd:
  case tokenArrayEnd:
    if (features_.allowDroppedNullPlaceholders_) {
      current_--;
      Value v;
      currentValue().swapPayload(v);
      currentValue().setOffsetStart(current_ - begin_ - 1);
      currentValue().setOffsetLimit(current_ - begin_);
      break;
    }
  default:
    currentValue().setOffsetStart(token.start_ - begin_);
    currentValue().setOffsetLimit(token.end_ - begin_);
    return addError("Syntax error: value, object or array expected.", token);
  }

  if (collectComments_) {
    lastValueEnd_ = current_;
    lastValueHasAComment_ = false;
    lastValue_ = &currentValue();
  }

  return successful;
}

/*!
Reads tokens from the input, skipping comments if allowed by the parser's features.
Continues reading until a non-comment token is found or the end of input is reached, utilizing the readToken method for token processing.
*/
bool OurReader::readTokenSkippingComments(Token& token) {
  bool success = readToken(token);
  if (features_.allowComments_) {
    while (success && token.type_ == tokenComment) {
      success = readToken(token);
    }
  }
  return success;
}

/*!
Reads and identifies the next token in the JSON input stream, handling various JSON elements such as delimiters, strings, numbers, boolean values, null, and special floating-point values.
Advances the current position in the stream and sets the token type and position based on the identified element.
*/
bool OurReader::readToken(Token& token) {
  skipSpaces();
  token.start_ = current_;
  Char c = getNextChar();
  bool ok = true;
  switch (c) {
  case '{':
    token.type_ = tokenObjectBegin;
    break;
  case '}':
    token.type_ = tokenObjectEnd;
    break;
  case '[':
    token.type_ = tokenArrayBegin;
    break;
  case ']':
    token.type_ = tokenArrayEnd;
    break;
  case '"':
    token.type_ = tokenString;
    ok = readString();
    break;
  case '\'':
    if (features_.allowSingleQuotes_) {
      token.type_ = tokenString;
      ok = readStringSingleQuote();
    } else {
      ok = false;
    }
    break;
  case '/':
    token.type_ = tokenComment;
    ok = readComment();
    break;
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    token.type_ = tokenNumber;
    readNumber(false);
    break;
  case '-':
    if (readNumber(true)) {
      token.type_ = tokenNumber;
    } else {
      token.type_ = tokenNegInf;
      ok = features_.allowSpecialFloats_ && match("nfinity", 7);
    }
    break;
  case '+':
    if (readNumber(true)) {
      token.type_ = tokenNumber;
    } else {
      token.type_ = tokenPosInf;
      ok = features_.allowSpecialFloats_ && match("nfinity", 7);
    }
    break;
  case 't':
    token.type_ = tokenTrue;
    ok = match("rue", 3);
    break;
  case 'f':
    token.type_ = tokenFalse;
    ok = match("alse", 4);
    break;
  case 'n':
    token.type_ = tokenNull;
    ok = match("ull", 3);
    break;
  case 'N':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenNaN;
      ok = match("aN", 2);
    } else {
      ok = false;
    }
    break;
  case 'I':
    if (features_.allowSpecialFloats_) {
      token.type_ = tokenPosInf;
      ok = match("nfinity", 7);
    } else {
      ok = false;
    }
    break;
  case ',':
    token.type_ = tokenArraySeparator;
    break;
  case ':':
    token.type_ = tokenMemberSeparator;
    break;
  case 0:
    token.type_ = tokenEndOfStream;
    break;
  default:
    ok = false;
    break;
  }
  if (!ok)
    token.type_ = tokenError;
  token.end_ = current_;
  return ok;
}

/*!
Advances the current position in the input stream, skipping over whitespace characters (spaces, tabs, carriage returns, and newlines) until a non-whitespace character is encountered or the end of the stream is reached.
*/
void OurReader::skipSpaces() {
  while (current_ != end_) {
    Char c = *current_;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      ++current_;
    else
      break;
  }
}

/*!
Checks for and skips the UTF-8 Byte Order Mark (BOM) at the beginning of the input if the skipBom flag is set.
Advances the begin and current pointers by 3 bytes if a BOM is detected.
*/
void OurReader::skipBom(bool skipBom) {
  if (skipBom) {
    if ((end_ - begin_) >= 3 && strncmp(begin_, "\xEF\xBB\xBF", 3) == 0) {
      begin_ += 3;
      current_ = begin_;
    }
  }
}

/*!
Compares the current input position with a given pattern, advancing the position if a match is found.
Ensures sufficient remaining input length before performing character-by-character comparison.
*/
bool OurReader::match(const Char* pattern, int patternLength) {
  if (end_ - current_ < patternLength)
    return false;
  int index = patternLength;
  while (index--)
    if (current_[index] != pattern[index])
      return false;
  current_ += patternLength;
  return true;
}

/*!
Identifies and processes C-style or C++-style comments in the JSON input.
Determines comment placement relative to the last parsed value and optionally collects comments if enabled.
Returns true if a comment was successfully read and processed.
*/
bool OurReader::readComment() {
  const Location commentBegin = current_ - 1;
  const Char c = getNextChar();
  bool successful = false;
  bool cStyleWithEmbeddedNewline = false;

  const bool isCStyleComment = (c == '*');
  const bool isCppStyleComment = (c == '/');
  if (isCStyleComment) {
    successful = readCStyleComment(&cStyleWithEmbeddedNewline);
  } else if (isCppStyleComment) {
    successful = readCppStyleComment();
  }

  if (!successful)
    return false;

  if (collectComments_) {
    CommentPlacement placement = commentBefore;

    if (!lastValueHasAComment_) {
      if (lastValueEnd_ && !containsNewLine(lastValueEnd_, commentBegin)) {
        if (isCppStyleComment || !cStyleWithEmbeddedNewline) {
          placement = commentAfterOnSameLine;
          lastValueHasAComment_ = true;
        }
      }
    }

    addComment(commentBegin, current_, placement);
  }
  return true;
}

/*!
Processes the input string, converting all line endings (CR, LF, or CRLF) to a consistent LF format.
Creates and returns a new string with normalized line endings, preserving all other characters from the original input.
*/
String OurReader::normalizeEOL(OurReader::Location begin,
                               OurReader::Location end) {
  String normalized;
  normalized.reserve(static_cast<size_t>(end - begin));
  OurReader::Location current = begin;
  while (current != end) {
    char c = *current++;
    if (c == '\r') {
      if (current != end && *current == '\n')
        ++current;
      normalized += '\n';
    } else {
      normalized += c;
    }
  }
  return normalized;
}

/*!
Processes and stores a comment in the JSON structure.
Normalizes the comment's line endings and either associates it with the last parsed value or adds it to a collection of comments for the next value, depending on the specified placement.
*/
void OurReader::addComment(Location begin, Location end,
                           CommentPlacement placement) {
  assert(collectComments_);
  const String& normalized = normalizeEOL(begin, end);
  if (placement == commentAfterOnSameLine) {
    assert(lastValue_ != nullptr);
    lastValue_->setComment(normalized, placement);
  } else {
    commentsBefore_ += normalized;
  }
}

/*!
Parses a C-style comment, advancing the current position to the comment's end.
Checks for newline characters within the comment and returns whether the comment was successfully terminated.
*/
bool OurReader::readCStyleComment(bool* containsNewLineResult) {
  *containsNewLineResult = false;

  while ((current_ + 1) < end_) {
    Char c = getNextChar();
    if (c == '*' && *current_ == '/')
      break;
    if (c == '\n')
      *containsNewLineResult = true;
  }

  return getNextChar() == '/';
}

/*!
Consumes characters until the end of the line, handling both Unix and Windows-style line endings.
Advances the current position using getNextChar() for each character processed.
*/
bool OurReader::readCppStyleComment() {
  while (current_ != end_) {
    Char c = getNextChar();
    if (c == '\n')
      break;
    if (c == '\r') {
      if (current_ != end_ && *current_ == '\n')
        getNextChar();
      break;
    }
  }
  return true;
}

/*!
Parses a JSON number from the current position, handling integers, floating-point values, and exponents.
Advances the current position to the end of the number if successful, and optionally checks for the 'Infinity' literal.
*/
bool OurReader::readNumber(bool checkInf) {
  Location p = current_;
  if (checkInf && p != end_ && *p == 'I') {
    current_ = ++p;
    return false;
  }
  char c = '0';
  while (c >= '0' && c <= '9')
    c = (current_ = p) < end_ ? *p++ : '\0';
  if (c == '.') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  if (c == 'e' || c == 'E') {
    c = (current_ = p) < end_ ? *p++ : '\0';
    if (c == '+' || c == '-')
      c = (current_ = p) < end_ ? *p++ : '\0';
    while (c >= '0' && c <= '9')
      c = (current_ = p) < end_ ? *p++ : '\0';
  }
  return true;
}
/*!
Processes characters in the input stream until encountering a closing double quote, handling escape sequences without interpreting them.
Returns true if a complete string is read, false otherwise.
*/
bool OurReader::readString() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '"')
      break;
  }
  return c == '"';
}

/*!
Processes characters within a single-quoted string, handling escape sequences and advancing through the input until a closing single quote is encountered or the end of input is reached.
Returns true if the string was properly terminated.
*/
bool OurReader::readStringSingleQuote() {
  Char c = 0;
  while (current_ != end_) {
    c = getNextChar();
    if (c == '\\')
      getNextChar();
    else if (c == '\'')
      break;
  }
  return c == '\'';
}

/*!
Parses a JSON object by reading key-value pairs, handling features like trailing commas and numeric keys.
Populates the current Value object with parsed data, managing errors and recovery as needed.
*/
bool OurReader::readObject(Token& token) {
  Token tokenName;
  String name;
  Value init(objectValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  while (readTokenSkippingComments(tokenName)) {
    if (tokenName.type_ == tokenObjectEnd &&
        (name.empty() || features_.allowTrailingCommas_))
      return true;
    name.clear();
    if (tokenName.type_ == tokenString) {
      if (!decodeString(tokenName, name))
        return recoverFromError(tokenObjectEnd);
    } else if (tokenName.type_ == tokenNumber && features_.allowNumericKeys_) {
      Value numberName;
      if (!decodeNumber(tokenName, numberName))
        return recoverFromError(tokenObjectEnd);
      name = numberName.asString();
    } else {
      break;
    }
    if (name.length() >= (1U << 30))
      throwRuntimeError("keylength >= 2^30");
    if (features_.rejectDupKeys_ && currentValue().isMember(name)) {
      String msg = "Duplicate key: '" + name + "'";
      return addErrorAndRecover(msg, tokenName, tokenObjectEnd);
    }

    Token colon;
    if (!readToken(colon) || colon.type_ != tokenMemberSeparator) {
      return addErrorAndRecover("Missing ':' after object member name", colon,
                                tokenObjectEnd);
    }
    Value& value = currentValue()[name];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok)
      return recoverFromError(tokenObjectEnd);

    Token comma;
    if (!readTokenSkippingComments(comma) ||
        (comma.type_ != tokenObjectEnd && comma.type_ != tokenArraySeparator)) {
      return addErrorAndRecover("Missing ',' or '}' in object declaration",
                                comma, tokenObjectEnd);
    }
    if (comma.type_ == tokenObjectEnd)
      return true;
  }
  return addErrorAndRecover("Missing '}' or object member name", tokenName,
                            tokenObjectEnd);
}

/*!
Parses a JSON array, iterating through its elements and handling nested values.
Supports features like trailing commas based on parser configuration.
Manages array construction, error recovery, and proper closing of the array structure.
*/
bool OurReader::readArray(Token& token) {
  Value init(arrayValue);
  currentValue().swapPayload(init);
  currentValue().setOffsetStart(token.start_ - begin_);
  int index = 0;
  for (;;) {
    skipSpaces();
    if (current_ != end_ && *current_ == ']' &&
        (index == 0 || (features_.allowTrailingCommas_ &&
                        !features_.allowDroppedNullPlaceholders_))) {
      Token endArray;
      readToken(endArray);
      return true;
    }
    Value& value = currentValue()[index++];
    nodes_.push(&value);
    bool ok = readValue();
    nodes_.pop();
    if (!ok)
      return recoverFromError(tokenArrayEnd);

    Token currentToken;
    ok = readTokenSkippingComments(currentToken);
    bool badTokenType = (currentToken.type_ != tokenArraySeparator &&
                         currentToken.type_ != tokenArrayEnd);
    if (!ok || badTokenType) {
      return addErrorAndRecover("Missing ',' or ']' in array declaration",
                                currentToken, tokenArrayEnd);
    }
    if (currentToken.type_ == tokenArrayEnd)
      break;
  }
  return true;
}

/*!
Decodes the numeric token into the current JSON value, updating its payload and offset information.
Utilizes a helper function for the actual number parsing and returns the success status of the operation.
*/
bool OurReader::decodeNumber(Token& token) {
  Value decoded;
  if (!decodeNumber(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Parses and converts a JSON number token into an integer or unsigned integer value.
Handles both positive and negative numbers, switching to double parsing if the number exceeds integer limits or contains non-digit characters.
*/
bool OurReader::decodeNumber(Token& token, Value& decoded) {
  Location current = token.start_;
  const bool isNegative = *current == '-';
  if (isNegative) {
    ++current;
  }

  static_assert(Value::maxLargestInt <= Value::maxLargestUInt,
                "Int must be smaller than UInt");

  static_assert(Value::minLargestInt <= -Value::maxLargestInt,
                "The absolute value of minLargestInt must be greater than or "
                "equal to maxLargestInt");
  static_assert(Value::minLargestInt / 10 >= -Value::maxLargestInt,
                "The absolute value of minLargestInt must be only 1 magnitude "
                "larger than maxLargest Int");

  static constexpr Value::LargestUInt positive_threshold =
      Value::maxLargestUInt / 10;
  static constexpr Value::UInt positive_last_digit = Value::maxLargestUInt % 10;

  static constexpr auto negative_threshold =
      Value::LargestUInt(-(Value::minLargestInt / 10));
  static constexpr auto negative_last_digit =
      Value::UInt(-(Value::minLargestInt % 10));

  const Value::LargestUInt threshold =
      isNegative ? negative_threshold : positive_threshold;
  const Value::UInt max_last_digit =
      isNegative ? negative_last_digit : positive_last_digit;

  Value::LargestUInt value = 0;
  while (current < token.end_) {
    Char c = *current++;
    if (c < '0' || c > '9')
      return decodeDouble(token, decoded);

    const auto digit(static_cast<Value::UInt>(c - '0'));
    if (value >= threshold) {
      if (value > threshold || current != token.end_ ||
          digit > max_last_digit) {
        return decodeDouble(token, decoded);
      }
    }
    value = value * 10 + digit;
  }

  if (isNegative) {
    const auto last_digit = static_cast<Value::UInt>(value % 10);
    decoded = -Value::LargestInt(value / 10) * 10 - last_digit;
  } else if (value <= Value::LargestUInt(Value::maxLargestInt)) {
    decoded = Value::LargestInt(value);
  } else {
    decoded = value;
  }

  return true;
}

/*!
Decodes a double-precision floating-point number from the given token and updates the current value with the decoded result.
Sets the offset information for the decoded value based on the token's position in the input stream.
*/
bool OurReader::decodeDouble(Token& token) {
  Value decoded;
  if (!decodeDouble(token, decoded))
    return false;
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Attempts to parse a double value from the given token's string representation.
Handles special cases like infinity and performs error checking.
Sets the decoded value if successful and returns a boolean indicating the outcome.
*/
bool OurReader::decodeDouble(Token& token, Value& decoded) {
  double value = 0;
  IStringStream is(String(token.start_, token.end_));
  if (!(is >> value)) {
    if (value == std::numeric_limits<double>::max())
      value = std::numeric_limits<double>::infinity();
    else if (value == std::numeric_limits<double>::lowest())
      value = -std::numeric_limits<double>::infinity();
    else if (!std::isinf(value))
      return addError(
          "'" + String(token.start_, token.end_) + "' is not a number.", token);
  }
  decoded = value;
  return true;
}

/*!
Decodes the JSON string token, creates a new Value object with the decoded string, and updates the current value with the decoded content.
Sets the offset start and limit for the current value based on the token's position in the input stream.
*/
bool OurReader::decodeString(Token& token) {
  String decoded_string;
  if (!decodeString(token, decoded_string))
    return false;
  Value decoded(decoded_string);
  currentValue().swapPayload(decoded);
  currentValue().setOffsetStart(token.start_ - begin_);
  currentValue().setOffsetLimit(token.end_ - begin_);
  return true;
}

/*!
Processes the input JSON string token, handling escape sequences and Unicode code points.
Iterates through the string, decoding special characters and escape sequences, and converts Unicode code points to UTF-8, storing the result in the output string.
*/
bool OurReader::decodeString(Token& token, String& decoded) {
  decoded.reserve(static_cast<size_t>(token.end_ - token.start_ - 2));
  Location current = token.start_ + 1;
  Location end = token.end_ - 1;
  while (current != end) {
    Char c = *current++;
    if (c == '"')
      break;
    if (c == '\\') {
      if (current == end)
        return addError("Empty escape sequence in string", token, current);
      Char escape = *current++;
      switch (escape) {
      case '"':
        decoded += '"';
        break;
      case '/':
        decoded += '/';
        break;
      case '\\':
        decoded += '\\';
        break;
      case 'b':
        decoded += '\b';
        break;
      case 'f':
        decoded += '\f';
        break;
      case 'n':
        decoded += '\n';
        break;
      case 'r':
        decoded += '\r';
        break;
      case 't':
        decoded += '\t';
        break;
      case 'u': {
        unsigned int unicode;
        if (!decodeUnicodeCodePoint(token, current, end, unicode))
          return false;
        decoded += codePointToUTF8(unicode);
      } break;
      default:
        return addError("Bad escape sequence in string", token, current);
      }
    } else {
      decoded += c;
    }
  }
  return true;
}

/*!
Decodes a Unicode code point from a JSON string, handling both single code points and surrogate pairs.
Processes Unicode escape sequences and ensures proper representation of international characters within JSON data.
*/
bool OurReader::decodeUnicodeCodePoint(Token& token, Location& current,
                                       Location end, unsigned int& unicode) {

  if (!decodeUnicodeEscapeSequence(token, current, end, unicode))
    return false;
  if (unicode >= 0xD800 && unicode <= 0xDBFF) {
    if (end - current < 6)
      return addError(
          "additional six characters expected to parse unicode surrogate pair.",
          token, current);
    if (*(current++) == '\\' && *(current++) == 'u') {
      unsigned int surrogatePair;
      if (decodeUnicodeEscapeSequence(token, current, end, surrogatePair)) {
        unicode = 0x10000 + ((unicode & 0x3FF) << 10) + (surrogatePair & 0x3FF);
      } else
        return false;
    } else
      return addError("expecting another \\u token to begin the second half of "
                      "a unicode surrogate pair",
                      token, current);
  }
  return true;
}

/*!
Parses a four-digit hexadecimal Unicode escape sequence, converting it to its corresponding integer value.
Validates the input, performs the conversion digit by digit, and reports errors for invalid sequences or insufficient digits.
*/
bool OurReader::decodeUnicodeEscapeSequence(Token& token, Location& current,
                                            Location end,
                                            unsigned int& ret_unicode) {
  if (end - current < 4)
    return addError(
        "Bad unicode escape sequence in string: four digits expected.", token,
        current);
  int unicode = 0;
  for (int index = 0; index < 4; ++index) {
    Char c = *current++;
    unicode *= 16;
    if (c >= '0' && c <= '9')
      unicode += c - '0';
    else if (c >= 'a' && c <= 'f')
      unicode += c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      unicode += c - 'A' + 10;
    else
      return addError(
          "Bad unicode escape sequence in string: hexadecimal digit expected.",
          token, current);
  }
  ret_unicode = static_cast<unsigned int>(unicode);
  return true;
}

/*!
Adds an error to the internal errors collection by creating an ErrorInfo object with the provided message, token, and extra location.
Returns false to indicate an error condition to the caller.
*/
bool OurReader::addError(const String& message, Token& token, Location extra) {
  ErrorInfo info;
  info.token_ = token;
  info.message_ = message;
  info.extra_ = extra;
  errors_.push_back(info);
  return false;
}

/*!
Skips tokens in the input stream until a specified token type or end of stream is reached.
Resets the error count to its original value before returning, effectively discarding any errors encountered during the recovery process.
*/
bool OurReader::recoverFromError(TokenType skipUntilToken) {
  size_t errorCount = errors_.size();
  Token skip;
  for (;;) {
    if (!readToken(skip))
      errors_.resize(errorCount);
    if (skip.type_ == skipUntilToken || skip.type_ == tokenEndOfStream)
      break;
  }
  errors_.resize(errorCount);
  return false;
}

/*!
Reports an error associated with the given token and attempts to recover by skipping tokens until the specified token type is reached.
Combines error reporting and recovery to allow parsing to continue after encountering JSON structure errors.
*/
bool OurReader::addErrorAndRecover(const String& message, Token& token,
                                   TokenType skipUntilToken) {
  addError(message, token);
  return recoverFromError(skipUntilToken);
}

/*!
Returns a reference to the top element of the nodes stack, representing the current JSON value being processed during parsing.
*/
Value& OurReader::currentValue() { return *(nodes_.top()); }

/*!
Retrieves and returns the next character from the input stream, advancing the current position.
Returns 0 if the end of the stream is reached.
*/
OurReader::Char OurReader::getNextChar() {
  if (current_ == end_)
    return 0;
  return *current_++;
}

/*!
Calculates the line and column numbers for a given location in the JSON document by iterating through the input, tracking line breaks and updating counters.
Uses character-by-character analysis to determine precise positioning for error reporting purposes.
*/
void OurReader::getLocationLineAndColumn(Location location, int& line,
                                         int& column) const {
  Location current = begin_;
  Location lastLineStart = current;
  line = 0;
  while (current < location && current != end_) {
    Char c = *current++;
    if (c == '\r') {
      if (current != end_ && *current == '\n')
        ++current;
      lastLineStart = current;
      ++line;
    } else if (c == '\n') {
      lastLineStart = current;
      ++line;
    }
  }
  column = int(location - lastLineStart) + 1;
  ++line;
}

/*!
Converts the given location to a human-readable string representation of line and column numbers.
Utilizes an internal method to calculate the precise position, then formats the result into a concise string.
*/
String OurReader::getLocationLineAndColumn(Location location) const {
  int line, column;
  getLocationLineAndColumn(location, line, column);
  char buffer[18 + 16 + 16 + 1];
  jsoncpp_snprintf(buffer, sizeof(buffer), "Line %d, Column %d", line, column);
  return buffer;
}

/*!
Compiles and formats all parsing errors into a single string.
Iterates through the error list, adding location information, error messages, and additional details for each error encountered during JSON processing.
*/
String OurReader::getFormattedErrorMessages() const {
  String formattedMessage;
  for (const auto& error : errors_) {
    formattedMessage +=
        "* " + getLocationLineAndColumn(error.token_.start_) + "\n";
    formattedMessage += "  " + error.message_ + "\n";
    if (error.extra_)
      formattedMessage +=
          "See " + getLocationLineAndColumn(error.extra_) + " for detail.\n";
  }
  return formattedMessage;
}

std::vector<CharReader::StructuredError>
/*!
Converts internal error representations to a standardized format, creating a vector of StructuredError objects.
Each object contains the start and end offsets of the erroneous token within the input stream, along with an associated error message.
*/
OurReader::getStructuredErrors() const {
  std::vector<CharReader::StructuredError> allErrors;
  for (const auto& error : errors_) {
    CharReader::StructuredError structured;
    structured.offset_start = error.token_.start_ - begin_;
    structured.offset_limit = error.token_.end_ - begin_;
    structured.message = error.message_;
    allErrors.push_back(structured);
  }
  return allErrors;
}

/*!
\class OurCharReader
\brief Implements a customizable JSON parser with advanced features.

Provides a concrete implementation of the CharReader interface for parsing JSON documents.
Offers customizable parsing behavior through comment collection and feature options, allowing for flexible and efficient JSON processing.
Encapsulates an internal implementation for actual parsing, making it suitable for various JSON parsing requirements in C++ applications.
*/
class OurCharReader : public CharReader {

public:
  /*!
  \brief Constructs a JSON parser with specified options.
  
  Creates an OurCharReader instance for parsing JSON documents.
  Initializes the underlying OurImpl object with the provided parsing options, allowing customization of comment handling and other parsing features.
  
  \param collectComments Flag indicating whether to collect comments during parsing.
  \param features Parsing options that control the behavior of the JSON reader.
  */
  OurCharReader(bool collectComments, OurFeatures const& features)
      : CharReader(
            std::unique_ptr<OurImpl>(new OurImpl(collectComments, features))) {}

protected:
  /*!
  \class OurImpl
  \brief Implements a JSON document parser with configurable features.
  
  Encapsulates an OurReader object for parsing JSON strings, handling errors, and optionally collecting comments.
  Provides methods for parsing JSON data, retrieving formatted error messages, and obtaining structured error information.
  Designed to be a key component in the JSON parsing process, offering flexibility and efficiency in JSON document processing.
  */
  class OurImpl : public Impl {
  public:
    /*!
    \brief Initializes a new OurImpl instance.
    
    Constructs an OurImpl object with specified comment collection behavior and parsing features.
    Sets up the internal OurReader with the provided features for JSON parsing.
    
    \param collectComments Flag indicating whether to collect comments during parsing.
    \param features OurFeatures object specifying the parsing features to be used.
    */
    OurImpl(bool collectComments, OurFeatures const& features)
        : collectComments_(collectComments), reader_(features) {}

    /*!
    \brief Parses a JSON document.
    
    Parses a JSON document using the internal OurReader object.
    Optionally collects comments and provides formatted error messages if parsing fails.
    
    \param beginDoc Pointer to the beginning of the JSON document string.
    \param endDoc Pointer to the end of the JSON document string.
    \param root Pointer to a Value object where the parsed JSON will be stored.
    \param errs Pointer to a String object where formatted error messages will be stored if parsing fails. If null, no error messages are stored.
    
    \return True if parsing was successful, false otherwise.
    */
    bool parse(char const* beginDoc, char const* endDoc, Value* root,
               String* errs) override {
      bool ok = reader_.parse(beginDoc, endDoc, *root, collectComments_);
      if (errs) {
        *errs = reader_.getFormattedErrorMessages();
      }
      return ok;
    }

    std::vector<CharReader::StructuredError>
    /*!
    \brief Retrieves structured error information.
    
    Delegates to the underlying OurReader object to obtain structured error information.
    This function provides access to detailed error data for more advanced error handling and reporting.
    
    \return A vector of structured error objects containing detailed information about parsing errors.
    */
    getStructuredErrors() const override {
      return reader_.getStructuredErrors();
    }

  private:
    bool const collectComments_;
    OurReader reader_;
  };
};

/*!
Initializes the CharReaderBuilder with default settings by calling setDefaults on the internal settings_ member.
This prepares the builder for creating CharReader objects with standard JSON parsing configurations.
*/
CharReaderBuilder::CharReaderBuilder() { setDefaults(&settings_); }
CharReaderBuilder::~CharReaderBuilder() = default;
/*!
Creates a new CharReader instance based on the current settings.
Configures parsing features such as comment handling, trailing commas, and numeric keys according to the builder's settings, then instantiates and returns a new OurCharReader object with these features.
*/
CharReader* CharReaderBuilder::newCharReader() const {
  bool collectComments = settings_["collectComments"].asBool();
  OurFeatures features = OurFeatures::all();
  features.allowComments_ = settings_["allowComments"].asBool();
  features.allowTrailingCommas_ = settings_["allowTrailingCommas"].asBool();
  features.strictRoot_ = settings_["strictRoot"].asBool();
  features.allowDroppedNullPlaceholders_ =
      settings_["allowDroppedNullPlaceholders"].asBool();
  features.allowNumericKeys_ = settings_["allowNumericKeys"].asBool();
  features.allowSingleQuotes_ = settings_["allowSingleQuotes"].asBool();

  features.stackLimit_ = static_cast<size_t>(settings_["stackLimit"].asUInt());
  features.failIfExtra_ = settings_["failIfExtra"].asBool();
  features.rejectDupKeys_ = settings_["rejectDupKeys"].asBool();
  features.allowSpecialFloats_ = settings_["allowSpecialFloats"].asBool();
  features.skipBom_ = settings_["skipBom"].asBool();
  return new OurCharReader(collectComments, features);
}

/*!
Validates the current settings against a predefined set of valid keys.
Populates the provided Json::Value with any invalid settings found, or returns false immediately if no Json::Value is provided and an invalid setting is encountered.
*/
bool CharReaderBuilder::validate(Json::Value* invalid) const {
  static const auto& valid_keys = *new std::set<String>{
      "collectComments",
      "allowComments",
      "allowTrailingCommas",
      "strictRoot",
      "allowDroppedNullPlaceholders",
      "allowNumericKeys",
      "allowSingleQuotes",
      "stackLimit",
      "failIfExtra",
      "rejectDupKeys",
      "allowSpecialFloats",
      "skipBom",
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

Value& CharReaderBuilder::operator[](const String& key) {
  return settings_[key];
}
/*!
Configures parsing settings to enforce strict JSON rules by disabling lenient features and enabling stricter validation checks.
Modifies the provided settings object with predefined values for various parsing options.
*/
void CharReaderBuilder::strictMode(Json::Value* settings) {
  (*settings)["allowComments"] = false;
  (*settings)["allowTrailingCommas"] = false;
  (*settings)["strictRoot"] = true;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = true;
  (*settings)["rejectDupKeys"] = true;
  (*settings)["allowSpecialFloats"] = false;
  (*settings)["skipBom"] = true;
}
/*!
Populates the provided Json::Value object with default configuration settings for JSON parsing.
Sets various options such as enabling comments and trailing commas, disabling strict root parsing, and configuring other parsing behaviors to establish a consistent baseline for CharReader instances.
*/
void CharReaderBuilder::setDefaults(Json::Value* settings) {
  (*settings)["collectComments"] = true;
  (*settings)["allowComments"] = true;
  (*settings)["allowTrailingCommas"] = true;
  (*settings)["strictRoot"] = false;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = false;
  (*settings)["rejectDupKeys"] = false;
  (*settings)["allowSpecialFloats"] = false;
  (*settings)["skipBom"] = true;
}
/*!
Configures the provided settings object for strict ECMA-404 JSON parsing.
Disables all non-standard features and sets specific parsing options to ensure compliance with the ECMA-404 JSON standard.
*/
void CharReaderBuilder::ecma404Mode(Json::Value* settings) {
  (*settings)["allowComments"] = false;
  (*settings)["allowTrailingCommas"] = false;
  (*settings)["strictRoot"] = false;
  (*settings)["allowDroppedNullPlaceholders"] = false;
  (*settings)["allowNumericKeys"] = false;
  (*settings)["allowSingleQuotes"] = false;
  (*settings)["stackLimit"] = 1000;
  (*settings)["failIfExtra"] = true;
  (*settings)["rejectDupKeys"] = false;
  (*settings)["allowSpecialFloats"] = false;
  (*settings)["skipBom"] = false;
}

std::vector<CharReader::StructuredError>
/*!
Returns a collection of structured error objects containing detailed information about JSON parsing errors.
Delegates the retrieval to the implementation object.
*/
CharReader::getStructuredErrors() const {
  return _impl->getStructuredErrors();
}

/*!
Delegates the parsing of a JSON document to the internal implementation.
Reads the document from the specified character range, constructs a Value object, and reports any errors encountered during parsing.
*/
bool CharReader::parse(char const* beginDoc, char const* endDoc, Value* root,
                       String* errs) {
  return _impl->parse(beginDoc, endDoc, root, errs);
}

/*!
Reads JSON data from an input stream, converts it to a string, and uses a CharReader to parse the content into a Value object.
Returns the result of the parsing operation.
*/
bool parseFromStream(CharReader::Factory const& fact, IStream& sin, Value* root,
                     String* errs) {
  OStringStream ssin;
  ssin << sin.rdbuf();
  String doc = std::move(ssin).str();
  char const* begin = doc.data();
  char const* end = begin + doc.size();
  CharReaderPtr const reader(fact.newCharReader());
  return reader->parse(begin, end, root, errs);
}

IStream& operator>>(IStream& sin, Value& root) {
  CharReaderBuilder b;
  String errs;
  bool ok = parseFromStream(b, sin, &root, &errs);
  if (!ok) {
    throwRuntimeError(errs);
  }
  return sin;
}

} // namespace Json
