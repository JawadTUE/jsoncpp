#if !defined(JSON_IS_AMALGAMATION)
#include <json/assertions.h>
#include <json/value.h>
#include <json/writer.h>
#endif
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

#if defined(_MSC_VER) && _MSC_VER < 1900
#include <stdarg.h>
/*!
\brief Implements C99-compliant vsnprintf functionality for pre-1900 MSVC compilers.

Formats a string according to the provided format and arguments, handling buffer size constraints.
Utilizes MSVC-specific functions to achieve C99-compatible behavior, addressing compatibility issues in older Microsoft Visual C++ environments.

\param outBuf The output buffer where the formatted string will be written.
\param size The maximum number of characters to be written to the output buffer.
\param format The format string specifying how subsequent arguments are converted for output.
\param ap A variable argument list containing the arguments to be formatted.

\return The number of characters that would have been written if size had been sufficiently large, not counting the terminating null character.
*/
static int msvc_pre1900_c99_vsnprintf(char* outBuf, size_t size,
                                      const char* format, va_list ap) {
  int count = -1;
  if (size != 0)
    count = _vsnprintf_s(outBuf, size, _TRUNCATE, format, ap);
  if (count == -1)
    count = _vscprintf(format, ap);
  return count;
}

int JSON_API msvc_pre1900_c99_snprintf(char* outBuf, size_t size,
                                       const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  const int count = msvc_pre1900_c99_vsnprintf(outBuf, size, format, ap);
  va_end(ap);
  return count;
}
#endif

#if defined(_MSC_VER)
#pragma warning(disable : 4702)
#endif

#define JSON_ASSERT_UNREACHABLE assert(false)

namespace Json {
template <typename T>
/*!
\brief Creates a deep copy of a unique_ptr.

Creates and returns a new unique_ptr that owns a copy of the object managed by the input unique_ptr.
If the input is null, returns a null unique_ptr.

\param p The unique_ptr to be cloned. May be null.

\return A new unique_ptr owning a copy of the object in 'p', or null if 'p' is null.
*/
static std::unique_ptr<T> cloneUnique(const std::unique_ptr<T>& p) {
  std::unique_ptr<T> r;
  if (p) {
    r = std::unique_ptr<T>(new T(*p));
  }
  return r;
}

#if defined(__ARMEL__)
#define ALIGNAS(byte_alignment) __attribute__((aligned(byte_alignment)))
#else
#define ALIGNAS(byte_alignment)
#endif

/*!
Returns a reference to a static null JSON value.
This singleton serves as a sentinel for invalid or non-existent JSON elements during parsing and navigation operations.
*/
Value const& Value::nullSingleton() {
  static Value const nullStatic;
  return nullStatic;
}

#if JSON_USE_NULLREF
Value const& Value::null = Value::nullSingleton();

Value const& Value::nullRef = Value::nullSingleton();
#endif

#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
template <typename T, typename U>
/*!
\brief Checks if a value is within a specified range.

Determines if a double value falls within a given range, inclusive of the minimum and maximum values.
Performs additional checks to handle edge cases and ensure type safety during numeric conversions.

\param d The double value to check.
\param min The minimum value of the range.
\param max The maximum value of the range.

\return True if the value is within the range, false otherwise.
*/
static inline bool InRange(double d, T min, U max) {
  return d >= static_cast<double>(min) && d <= static_cast<double>(max) &&
         !(static_cast<U>(d) == min && d != static_cast<double>(min));
}
#else
static inline double integerToDouble(Json::UInt64 value) {
  return static_cast<double>(Int64(value / 2)) * 2.0 +
         static_cast<double>(Int64(value & 1));
}

template <typename T> static inline double integerToDouble(T value) {
  return static_cast<double>(value);
}

template <typename T, typename U>
static inline bool InRange(double d, T min, U max) {
  return d >= integerToDouble(min) && d <= integerToDouble(max) &&
         !(static_cast<U>(d) == min && d != integerToDouble(min));
}
#endif

/*!
\brief Duplicates a string value with a specified length.

Allocates memory for a new string, copies the input string up to the specified length, and null-terminates the result.
Throws a runtime error if memory allocation fails.

\param value The input string to be duplicated.
\param length The number of characters to copy from the input string. If greater than or equal to Value::maxInt, it is capped at Value::maxInt - 1.

\return A pointer to the newly allocated, null-terminated string containing the duplicated value.
*/
static inline char* duplicateStringValue(const char* value, size_t length) {
  if (length >= static_cast<size_t>(Value::maxInt))
    length = Value::maxInt - 1;

  auto newString = static_cast<char*>(malloc(length + 1));
  if (newString == nullptr) {
    throwRuntimeError("in Json::Value::duplicateStringValue(): "
                      "Failed to allocate string value buffer");
  }
  memcpy(newString, value, length);
  newString[length] = 0;
  return newString;
}

/*!
\brief Duplicates a string and prefixes it with its length.

Allocates memory for a new string, prefixes it with its length, copies the original string content, and adds a null terminator.
Ensures efficient string handling and memory management within the JSON structure.

\param value The original string to be duplicated.
\param length The length of the original string.

\return A pointer to the newly allocated string with prefixed length and null terminator.
*/
static inline char* duplicateAndPrefixStringValue(const char* value,
                                                  unsigned int length) {
  JSON_ASSERT_MESSAGE(length <= static_cast<unsigned>(Value::maxInt) -
                                    sizeof(unsigned) - 1U,
                      "in Json::Value::duplicateAndPrefixStringValue(): "
                      "length too big for prefixing");
  size_t actualLength = sizeof(length) + length + 1;
  auto newString = static_cast<char*>(malloc(actualLength));
  if (newString == nullptr) {
    throwRuntimeError("in Json::Value::duplicateAndPrefixStringValue(): "
                      "Failed to allocate string value buffer");
  }
  *reinterpret_cast<unsigned*>(newString) = length;
  memcpy(newString + sizeof(unsigned), value, length);
  newString[actualLength - 1U] = 0;
  return newString;
}
/*!
\brief Decodes a potentially prefixed string.

Extracts the length and actual content of a string that may or may not have a length prefix.
Used internally for efficient string handling in JSON value representations.

\param isPrefixed Boolean flag indicating whether the string is prefixed with its length.
\param prefixed The input string, which may be prefixed with its length.
\param length Pointer to store the length of the decoded string.
\param value Pointer to store the address of the actual string content.
*/
inline static void decodePrefixedString(bool isPrefixed, char const* prefixed,
                                        unsigned* length, char const** value) {
  if (!isPrefixed) {
    *length = static_cast<unsigned>(strlen(prefixed));
    *value = prefixed;
  } else {
    *length = *reinterpret_cast<unsigned const*>(prefixed);
    *value = prefixed + sizeof(unsigned);
  }
}
#if JSONCPP_USE_SECURE_MEMORY
static inline void releasePrefixedStringValue(char* value) {
  unsigned length = 0;
  char const* valueDecoded;
  decodePrefixedString(true, value, &length, &valueDecoded);
  size_t const size = sizeof(unsigned) + length + 1U;
  memset(value, 0, size);
  free(value);
}
static inline void releaseStringValue(char* value, unsigned length) {
  size_t size = (length == 0) ? strlen(value) : length;
  memset(value, 0, size);
  free(value);
}
#else
/*!
\brief Deallocates memory for a prefixed string value.

Frees the memory allocated for a string value in the JSON library.
Used internally for memory management of dynamically allocated strings within Value objects.

\param value Pointer to the dynamically allocated string to be freed.
*/
static inline void releasePrefixedStringValue(char* value) { free(value); }
/*!
\brief Frees dynamically allocated string memory.

Releases the memory associated with a dynamically allocated string.
This function is a wrapper around the standard free() function, simplifying memory management for string values.

\param value Pointer to the dynamically allocated string to be freed.
*/
static inline void releaseStringValue(char* value, unsigned) { free(value); }
#endif

} // namespace Json

#if !defined(JSON_IS_AMALGAMATION)

#include "json_valueiterator.inl"
#endif

namespace Json {

#if JSON_USE_EXCEPTION
/*!
Initializes the exception with a custom error message, moving the provided message into the internal storage for efficient handling.
*/
Exception::Exception(String msg) : msg_(std::move(msg)) {}
Exception::~Exception() noexcept = default;
/*!
Returns a C-style string containing the error message associated with this JSON exception.
Provides direct access to the underlying error description stored in the exception object.
*/
char const* Exception::what() const noexcept { return msg_.c_str(); }
/*!
Initializes a RuntimeError instance with a custom error message, passing the message to the base Exception class constructor.
*/
RuntimeError::RuntimeError(String const& msg) : Exception(msg) {}
/*!
Initializes a LogicError exception with a custom error message, delegating the construction to the base Exception class.
*/
LogicError::LogicError(String const& msg) : Exception(msg) {}
JSONCPP_NORETURN void throwRuntimeError(String const& msg) {
  throw RuntimeError(msg);
}
JSONCPP_NORETURN void throwLogicError(String const& msg) {
  throw LogicError(msg);
}
#else
JSONCPP_NORETURN void throwRuntimeError(String const& msg) {
  std::cerr << msg << std::endl;
  abort();
}
JSONCPP_NORETURN void throwLogicError(String const& msg) {
  std::cerr << msg << std::endl;
  abort();
}
#endif

Value::CZString::CZString(ArrayIndex index) : cstr_(nullptr), index_(index) {}

Value::CZString::CZString(char const* str, unsigned length,
                          DuplicationPolicy allocate)
    : cstr_(str) {
  storage_.policy_ = allocate & 0x3;
  storage_.length_ = length & 0x3FFFFFFF;
}

Value::CZString::CZString(const CZString& other) {
  cstr_ = (other.storage_.policy_ != noDuplication && other.cstr_ != nullptr
               ? duplicateStringValue(other.cstr_, other.storage_.length_)
               : other.cstr_);
  storage_.policy_ =
      static_cast<unsigned>(
          other.cstr_
              ? (static_cast<DuplicationPolicy>(other.storage_.policy_) ==
                         noDuplication
                     ? noDuplication
                     : duplicate)
              : static_cast<DuplicationPolicy>(other.storage_.policy_)) &
      3U;
  storage_.length_ = other.storage_.length_;
}

Value::CZString::CZString(CZString&& other) noexcept
    : cstr_(other.cstr_), index_(other.index_) {
  other.cstr_ = nullptr;
}

Value::CZString::~CZString() {
  if (cstr_ && storage_.policy_ == duplicate) {
    releaseStringValue(const_cast<char*>(cstr_), storage_.length_ + 1U);
  }
}

void Value::CZString::swap(CZString& other) {
  std::swap(cstr_, other.cstr_);
  std::swap(index_, other.index_);
}

Value::CZString& Value::CZString::operator=(const CZString& other) {
  cstr_ = other.cstr_;
  index_ = other.index_;
  return *this;
}

Value::CZString& Value::CZString::operator=(CZString&& other) noexcept {
  cstr_ = other.cstr_;
  index_ = other.index_;
  other.cstr_ = nullptr;
  return *this;
}

bool Value::CZString::operator<(const CZString& other) const {
  if (!cstr_)
    return index_ < other.index_;
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  unsigned min_len = std::min<unsigned>(this_len, other_len);
  JSON_ASSERT(this->cstr_ && other.cstr_);
  int comp = memcmp(this->cstr_, other.cstr_, min_len);
  if (comp < 0)
    return true;
  if (comp > 0)
    return false;
  return (this_len < other_len);
}

bool Value::CZString::operator==(const CZString& other) const {
  if (!cstr_)
    return index_ == other.index_;
  unsigned this_len = this->storage_.length_;
  unsigned other_len = other.storage_.length_;
  if (this_len != other_len)
    return false;
  JSON_ASSERT(this->cstr_ && other.cstr_);
  int comp = memcmp(this->cstr_, other.cstr_, this_len);
  return comp == 0;
}

ArrayIndex Value::CZString::index() const { return index_; }

const char* Value::CZString::data() const { return cstr_; }
unsigned Value::CZString::length() const { return storage_.length_; }
bool Value::CZString::isStaticString() const {
  return storage_.policy_ == noDuplication;
}

/*!
Initializes a JSON value of the specified type, setting default values based on the type.
Creates an empty container for array and object types.
*/
Value::Value(ValueType type) {
  static char const emptyString[] = "";
  initBasic(type);
  switch (type) {
  case nullValue:
    break;
  case intValue:
  case uintValue:
    value_.int_ = 0;
    break;
  case realValue:
    value_.real_ = 0.0;
    break;
  case stringValue:
    value_.string_ = const_cast<char*>(static_cast<char const*>(emptyString));
    break;
  case arrayValue:
  case objectValue:
    value_.map_ = new ObjectValues();
    break;
  case booleanValue:
    value_.bool_ = false;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
}

/*!
Initializes the JSON value with the provided integer, setting the internal type to integer and storing the value in the appropriate member variable.
*/
Value::Value(Int value) {
  initBasic(intValue);
  value_.int_ = value;
}

/*!
Initializes the JSON value with an unsigned integer, setting the internal type to uintValue and storing the provided value.
*/
Value::Value(UInt value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#if defined(JSON_HAS_INT64)
/*!
Initializes the JSON value with a 64-bit integer.
Sets the internal type to integer and stores the provided value in the appropriate member variable.
*/
Value::Value(Int64 value) {
  initBasic(intValue);
  value_.int_ = value;
}
/*!
Initializes the JSON value with an unsigned 64-bit integer.
Sets the internal type to uintValue and stores the provided value in the uint_ field of the union.
*/
Value::Value(UInt64 value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#endif

/*!
Initializes the JSON value with a double-precision floating-point number.
Sets the internal type to real and stores the provided double value.
*/
Value::Value(double value) {
  initBasic(realValue);
  value_.real_ = value;
}

/*!
Creates a string-type Value object by copying the provided C-style string.
Initializes the object's basic properties and uses a helper function to duplicate and prefix the input string with its length for efficient internal storage.
*/
Value::Value(const char* value) {
  initBasic(stringValue, true);
  JSON_ASSERT_MESSAGE(value != nullptr,
                      "Null Value Passed to Value Constructor");
  value_.string_ = duplicateAndPrefixStringValue(
      value, static_cast<unsigned>(strlen(value)));
}

/*!
Creates a JSON string value by duplicating and prefixing the character range with its length.
Initializes the value as a string type and manages memory allocation for efficient string handling within the JSON structure.
*/
Value::Value(const char* begin, const char* end) {
  initBasic(stringValue, true);
  value_.string_ =
      duplicateAndPrefixStringValue(begin, static_cast<unsigned>(end - begin));
}

/*!
Initializes a JSON string value by duplicating the input string and prefixing it with its length.
Sets the internal type to string and allocates memory for efficient string handling within the JSON structure.
*/
Value::Value(const String& value) {
  initBasic(stringValue, true);
  value_.string_ = duplicateAndPrefixStringValue(
      value.data(), static_cast<unsigned>(value.length()));
}

/*!
Initializes the Value object with a string value, using the provided static string without copying its contents.
Sets the internal type to stringValue and assigns the string pointer directly for improved performance with constant string literals.
*/
Value::Value(const StaticString& value) {
  initBasic(stringValue);
  value_.string_ = const_cast<char*>(value.c_str());
}

/*!
Initializes a JSON value with a boolean type and sets its internal boolean representation to the provided value.
*/
Value::Value(bool value) {
  initBasic(booleanValue);
  value_.bool_ = value;
}

/*!
Performs a deep copy of the given Json::Value object by duplicating both its payload and metadata, ensuring an independent copy with separate memory allocation for dynamic data.
*/
Value::Value(const Value& other) {
  dupPayload(other);
  dupMeta(other);
}

/*!
Initializes the Value object to null and efficiently transfers the contents from the provided Value object using move semantics, leaving the source object in a valid but unspecified state.
*/
Value::Value(Value&& other) noexcept {
  initBasic(nullValue);
  swap(other);
}

/*!
Releases the payload and resets the internal value to zero, ensuring proper cleanup of resources associated with the JSON value.
*/
Value::~Value() {
  releasePayload();
  value_.uint_ = 0;
}

Value& Value::operator=(const Value& other) {
  Value(other).swap(*this);
  return *this;
}

Value& Value::operator=(Value&& other) noexcept {
  other.swap(*this);
  return *this;
}

/*!
Exchanges the internal data representation with another Value object by swapping the bits and value members.
This operation is performed efficiently without copying or allocating memory.
*/
void Value::swapPayload(Value& other) {
  std::swap(bits_, other.bits_);
  std::swap(value_, other.value_);
}

/*!
Releases the current payload and duplicates the payload of the provided Value object, effectively performing a deep copy of the JSON data.
*/
void Value::copyPayload(const Value& other) {
  releasePayload();
  dupPayload(other);
}

/*!
Exchanges the contents of this Value object with another, including the main payload, comments, and position information.
Uses std::swap for efficient swapping of member variables.
*/
void Value::swap(Value& other) {
  swapPayload(other);
  std::swap(comments_, other.comments_);
  std::swap(start_, other.start_);
  std::swap(limit_, other.limit_);
}

/*!
Performs a deep copy of the provided Value object by copying both its payload and metadata.
Internally used for duplicating Value objects.
*/
void Value::copy(const Value& other) {
  copyPayload(other);
  dupMeta(other);
}

/*!
Returns the type of the held JSON value by casting the internal value_type_ member to ValueType.
This allows for efficient type checking and processing of JSON data.
*/
ValueType Value::type() const {
  return static_cast<ValueType>(bits_.value_type_);
}

/*!
Performs a three-way comparison with another JSON value, returning -1 if less than, 1 if greater than, or 0 if equal.
The comparison is based on the JSON value types and their contents.
*/
int Value::compare(const Value& other) const {
  if (*this < other)
    return -1;
  if (*this > other)
    return 1;
  return 0;
}

bool Value::operator<(const Value& other) const {
  int typeDelta = type() - other.type();
  if (typeDelta)
    return typeDelta < 0;
  switch (type()) {
  case nullValue:
    return false;
  case intValue:
    return value_.int_ < other.value_.int_;
  case uintValue:
    return value_.uint_ < other.value_.uint_;
  case realValue:
    return value_.real_ < other.value_.real_;
  case booleanValue:
    return value_.bool_ < other.value_.bool_;
  case stringValue: {
    if ((value_.string_ == nullptr) || (other.value_.string_ == nullptr)) {
      return other.value_.string_ != nullptr;
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->isAllocated(), this->value_.string_, &this_len,
                         &this_str);
    decodePrefixedString(other.isAllocated(), other.value_.string_, &other_len,
                         &other_str);
    unsigned min_len = std::min<unsigned>(this_len, other_len);
    JSON_ASSERT(this_str && other_str);
    int comp = memcmp(this_str, other_str, min_len);
    if (comp < 0)
      return true;
    if (comp > 0)
      return false;
    return (this_len < other_len);
  }
  case arrayValue:
  case objectValue: {
    auto thisSize = value_.map_->size();
    auto otherSize = other.value_.map_->size();
    if (thisSize != otherSize)
      return thisSize < otherSize;
    return (*value_.map_) < (*other.value_.map_);
  }
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false;
}

bool Value::operator<=(const Value& other) const { return !(other < *this); }

bool Value::operator>=(const Value& other) const { return !(*this < other); }

bool Value::operator>(const Value& other) const { return other < *this; }

bool Value::operator==(const Value& other) const {
  if (type() != other.type())
    return false;
  switch (type()) {
  case nullValue:
    return true;
  case intValue:
    return value_.int_ == other.value_.int_;
  case uintValue:
    return value_.uint_ == other.value_.uint_;
  case realValue:
    return value_.real_ == other.value_.real_;
  case booleanValue:
    return value_.bool_ == other.value_.bool_;
  case stringValue: {
    if ((value_.string_ == nullptr) || (other.value_.string_ == nullptr)) {
      return (value_.string_ == other.value_.string_);
    }
    unsigned this_len;
    unsigned other_len;
    char const* this_str;
    char const* other_str;
    decodePrefixedString(this->isAllocated(), this->value_.string_, &this_len,
                         &this_str);
    decodePrefixedString(other.isAllocated(), other.value_.string_, &other_len,
                         &other_str);
    if (this_len != other_len)
      return false;
    JSON_ASSERT(this_str && other_str);
    int comp = memcmp(this_str, other_str, this_len);
    return comp == 0;
  }
  case arrayValue:
  case objectValue:
    return value_.map_->size() == other.value_.map_->size() &&
           (*value_.map_) == (*other.value_.map_);
  default:
    JSON_ASSERT_UNREACHABLE;
  }
  return false;
}

bool Value::operator!=(const Value& other) const { return !(*this == other); }

/*!
Retrieves the stored string value as a C-style string pointer, ensuring the Value object contains a string type.
Decodes the internal string representation using decodePrefixedString to extract the actual string content.
*/
const char* Value::asCString() const {
  JSON_ASSERT_MESSAGE(type() == stringValue,
                      "in Json::Value::asCString(): requires stringValue");
  if (value_.string_ == nullptr)
    return nullptr;
  unsigned this_len;
  char const* this_str;
  decodePrefixedString(this->isAllocated(), this->value_.string_, &this_len,
                       &this_str);
  return this_str;
}

#if JSONCPP_USE_SECURE_MEMORY
unsigned Value::getCStringLength() const {
  JSON_ASSERT_MESSAGE(type() == stringValue,
                      "in Json::Value::asCString(): requires stringValue");
  if (value_.string_ == 0)
    return 0;
  unsigned this_len;
  char const* this_str;
  decodePrefixedString(this->isAllocated(), this->value_.string_, &this_len,
                       &this_str);
  return this_len;
}
#endif

/*!
Extracts the string content from the JSON value if it is of string type.
Uses the `decodePrefixedString` function to efficiently set the begin and end pointers to the string's boundaries without copying the content.
*/
bool Value::getString(char const** begin, char const** end) const {
  if (type() != stringValue)
    return false;
  if (value_.string_ == nullptr)
    return false;
  unsigned length;
  decodePrefixedString(this->isAllocated(), this->value_.string_, &length,
                       begin);
  *end = *begin + length;
  return true;
}

/*!
Converts the JSON value to its string representation based on its type.
Handles null, string, boolean, integer, unsigned integer, and real number types, using appropriate conversion methods for each.
Throws an error for non-convertible types.
*/
String Value::asString() const {
  switch (type()) {
  case nullValue:
    return "";
  case stringValue: {
    if (value_.string_ == nullptr)
      return "";
    unsigned this_len;
    char const* this_str;
    decodePrefixedString(this->isAllocated(), this->value_.string_, &this_len,
                         &this_str);
    return String(this_str, this_len);
  }
  case booleanValue:
    return value_.bool_ ? "true" : "false";
  case intValue:
    return valueToString(value_.int_);
  case uintValue:
    return valueToString(value_.uint_);
  case realValue:
    return valueToString(value_.real_);
  default:
    JSON_FAIL_MESSAGE("Type is not convertible to string");
  }
}

/*!
Converts the current value to an integer, handling various input types.
Performs range checks and type conversions, throwing an exception if conversion is not possible.
Returns appropriate integer values for null and boolean types.
*/
Value::Int Value::asInt() const {
  switch (type()) {
  case intValue:
    JSON_ASSERT_MESSAGE(isInt(), "LargestInt out of Int range");
    return Int(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isInt(), "LargestUInt out of Int range");
    return Int(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, minInt, maxInt),
                        "double out of Int range");
    return Int(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to Int.");
}

/*!
Converts the value to an unsigned integer based on its type.
Handles various input types, performing range checks and type-specific conversions.
Throws an exception if the conversion is not possible or if the value is out of range for an unsigned integer.
*/
Value::UInt Value::asUInt() const {
  switch (type()) {
  case intValue:
    JSON_ASSERT_MESSAGE(isUInt(), "LargestInt out of UInt range");
    return UInt(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isUInt(), "LargestUInt out of UInt range");
    return UInt(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, 0u, maxUInt),
                        "double out of UInt range");
    return UInt(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to UInt.");
}

#if defined(JSON_HAS_INT64)

/*!
Converts the value to a 64-bit signed integer based on its current type.
Handles various internal types, performing necessary range checks and assertions to ensure valid conversion.
Returns the appropriate integer value or throws an exception if conversion is not possible.
*/
Value::Int64 Value::asInt64() const {
  switch (type()) {
  case intValue:
    return Int64(value_.int_);
  case uintValue:
    JSON_ASSERT_MESSAGE(isInt64(), "LargestUInt out of Int64 range");
    return Int64(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(
        value_.real_ != minInt64,
        "Double value is minInt64, precise value cannot be determined");
    JSON_ASSERT_MESSAGE(InRange(value_.real_, minInt64, maxInt64),
                        "double out of Int64 range");
    return Int64(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to Int64.");
}

/*!
Converts the current value to an unsigned 64-bit integer, handling various input types.
Performs range checks and type conversions, throwing an exception if the conversion is not possible or the value is out of range.
*/
Value::UInt64 Value::asUInt64() const {
  switch (type()) {
  case intValue:
    JSON_ASSERT_MESSAGE(isUInt64(), "LargestInt out of UInt64 range");
    return UInt64(value_.int_);
  case uintValue:
    return UInt64(value_.uint_);
  case realValue:
    JSON_ASSERT_MESSAGE(InRange(value_.real_, 0u, maxUInt64),
                        "double out of UInt64 range");
    return UInt64(value_.real_);
  case nullValue:
    return 0;
  case booleanValue:
    return value_.bool_ ? 1 : 0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to UInt64.");
}
#endif

/*!
Returns the value as the largest supported integer type, either int or int64_t depending on the build configuration.
Internally calls asInt() or asInt64() based on whether JSON_NO_INT64 is defined.
*/
LargestInt Value::asLargestInt() const {
#if defined(JSON_NO_INT64)
  return asInt();
#else
  return asInt64();
#endif
}

/*!
Retrieves the stored value as the largest supported unsigned integer type for the platform.
Delegates to either asUInt() or asUInt64() based on the JSON_NO_INT64 configuration.
*/
LargestUInt Value::asLargestUInt() const {
#if defined(JSON_NO_INT64)
  return asUInt();
#else
  return asUInt64();
#endif
}

/*!
Converts the internal value to a double-precision floating-point number, handling various types including integers, unsigned integers, real numbers, null, and boolean.
Throws an exception for non-convertible types.
*/
double Value::asDouble() const {
  switch (type()) {
  case intValue:
    return static_cast<double>(value_.int_);
  case uintValue:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<double>(value_.uint_);
#else
    return integerToDouble(value_.uint_);
#endif
  case realValue:
    return value_.real_;
  case nullValue:
    return 0.0;
  case booleanValue:
    return value_.bool_ ? 1.0 : 0.0;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to double.");
}

/*!
Converts the stored value to a float, handling various data types including integers, unsigned integers, floating-point numbers, null values, and booleans.
Throws an exception for non-convertible types.
*/
float Value::asFloat() const {
  switch (type()) {
  case intValue:
    return static_cast<float>(value_.int_);
  case uintValue:
#if !defined(JSON_USE_INT64_DOUBLE_CONVERSION)
    return static_cast<float>(value_.uint_);
#else
    return static_cast<float>(integerToDouble(value_.uint_));
#endif
  case realValue:
    return static_cast<float>(value_.real_);
  case nullValue:
    return 0.0;
  case booleanValue:
    return value_.bool_ ? 1.0F : 0.0F;
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to float.");
}

/*!
Converts the value to a boolean based on its type.
Returns true for non-zero numeric values and existing boolean values, false for null and zero values, and throws an error for unconvertible types.
*/
bool Value::asBool() const {
  switch (type()) {
  case booleanValue:
    return value_.bool_;
  case nullValue:
    return false;
  case intValue:
    return value_.int_ != 0;
  case uintValue:
    return value_.uint_ != 0;
  case realValue: {
    const auto value_classification = std::fpclassify(value_.real_);
    return value_classification != FP_ZERO && value_classification != FP_NAN;
  }
  default:
    break;
  }
  JSON_FAIL_MESSAGE("Value is not convertible to bool.");
}

/*!
Evaluates whether the current value can be converted to the specified ValueType.
Uses type-specific logic and range checks to determine convertibility based on the target type and the current value's type and content.
*/
bool Value::isConvertibleTo(ValueType other) const {
  switch (other) {
  case nullValue:
    return (isNumeric() && asDouble() == 0.0) ||
           (type() == booleanValue && !value_.bool_) ||
           (type() == stringValue && asString().empty()) ||
           (type() == arrayValue && value_.map_->empty()) ||
           (type() == objectValue && value_.map_->empty()) ||
           type() == nullValue;
  case intValue:
    return isInt() ||
           (type() == realValue && InRange(value_.real_, minInt, maxInt)) ||
           type() == booleanValue || type() == nullValue;
  case uintValue:
    return isUInt() ||
           (type() == realValue && InRange(value_.real_, 0u, maxUInt)) ||
           type() == booleanValue || type() == nullValue;
  case realValue:
    return isNumeric() || type() == booleanValue || type() == nullValue;
  case booleanValue:
    return isNumeric() || type() == booleanValue || type() == nullValue;
  case stringValue:
    return isNumeric() || type() == booleanValue || type() == stringValue ||
           type() == nullValue;
  case arrayValue:
    return type() == arrayValue || type() == nullValue;
  case objectValue:
    return type() == objectValue || type() == nullValue;
  }
  JSON_ASSERT_UNREACHABLE;
  return false;
}

/*!
Returns the number of elements in the JSON value based on its type.
For arrays, calculates the size using the last element's index; for objects, returns the number of key-value pairs; for all other types, returns 0.
*/
ArrayIndex Value::size() const {
  switch (type()) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
  case stringValue:
    return 0;
  case arrayValue:
    if (!value_.map_->empty()) {
      ObjectValues::const_iterator itLast = value_.map_->end();
      --itLast;
      return (*itLast).first.index() + 1;
    }
    return 0;
  case objectValue:
    return ArrayIndex(value_.map_->size());
  }
  JSON_ASSERT_UNREACHABLE;
  return 0;
}

/*!
Checks if the value is empty by evaluating its type and size.
Returns true for empty null values, arrays, or objects, and false for all other types.
*/
bool Value::empty() const {
  if (isNull() || isArray() || isObject())
    return size() == 0U;
  return false;
}

Value::operator bool() const { return !isNull(); }

/*!
Removes all elements from an array or object Value, leaving it empty.
Asserts if called on a null Value and has no effect on other Value types.
*/
void Value::clear() {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == arrayValue ||
                          type() == objectValue,
                      "in Json::Value::clear(): requires complex value");
  start_ = 0;
  limit_ = 0;
  switch (type()) {
  case arrayValue:
  case objectValue:
    value_.map_->clear();
    break;
  default:
    break;
  }
}

/*!
Resizes the array to the specified size, adding null elements if expanding or removing excess elements if shrinking.
Converts a null value to an empty array before resizing.
*/
void Value::resize(ArrayIndex newSize) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == arrayValue,
                      "in Json::Value::resize(): requires arrayValue");
  if (type() == nullValue)
    *this = Value(arrayValue);
  ArrayIndex oldSize = size();
  if (newSize == 0)
    clear();
  else if (newSize > oldSize)
    for (ArrayIndex i = oldSize; i < newSize; ++i)
      (*this)[i];
  else {
    for (ArrayIndex index = newSize; index < oldSize; ++index) {
      value_.map_->erase(index);
    }
    JSON_ASSERT(size() == newSize);
  }
}

Value& Value::operator[](ArrayIndex index) {
  JSON_ASSERT_MESSAGE(
      type() == nullValue || type() == arrayValue,
      "in Json::Value::operator[](ArrayIndex): requires arrayValue");
  if (type() == nullValue)
    *this = Value(arrayValue);
  CZString key(index);
  auto it = value_.map_->lower_bound(key);
  if (it != value_.map_->end() && (*it).first == key)
    return (*it).second;

  ObjectValues::value_type defaultValue(key, nullSingleton());
  it = value_.map_->insert(it, defaultValue);
  return (*it).second;
}

Value& Value::operator[](int index) {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in Json::Value::operator[](int index): index cannot be negative");
  return (*this)[ArrayIndex(index)];
}

const Value& Value::operator[](ArrayIndex index) const {
  JSON_ASSERT_MESSAGE(
      type() == nullValue || type() == arrayValue,
      "in Json::Value::operator[](ArrayIndex)const: requires arrayValue");
  if (type() == nullValue)
    return nullSingleton();
  CZString key(index);
  ObjectValues::const_iterator it = value_.map_->find(key);
  if (it == value_.map_->end())
    return nullSingleton();
  return (*it).second;
}

const Value& Value::operator[](int index) const {
  JSON_ASSERT_MESSAGE(
      index >= 0,
      "in Json::Value::operator[](int index) const: index cannot be negative");
  return (*this)[ArrayIndex(index)];
}

/*!
Initializes the basic properties of the Value object by setting its type, allocation status, and resetting comments and internal counters.
Used internally for basic initialization of Value instances.
*/
void Value::initBasic(ValueType type, bool allocated) {
  setType(type);
  setIsAllocated(allocated);
  comments_ = Comments{};
  start_ = 0;
  limit_ = 0;
}

/*!
Duplicates the payload of another Value object, performing a deep copy for complex types.
Handles different JSON value types, allocating memory as needed for strings, arrays, and objects while maintaining the original type and content.
*/
void Value::dupPayload(const Value& other) {
  setType(other.type());
  setIsAllocated(false);
  switch (type()) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
    value_ = other.value_;
    break;
  case stringValue:
    if (other.value_.string_ && other.isAllocated()) {
      unsigned len;
      char const* str;
      decodePrefixedString(other.isAllocated(), other.value_.string_, &len,
                           &str);
      value_.string_ = duplicateAndPrefixStringValue(str, len);
      setIsAllocated(true);
    } else {
      value_.string_ = other.value_.string_;
    }
    break;
  case arrayValue:
  case objectValue:
    value_.map_ = new ObjectValues(*other.value_.map_);
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
}

/*!
Frees memory resources based on the Value's current type.
For string types, releases allocated string memory if necessary, and for array or object types, deletes the associated map.
*/
void Value::releasePayload() {
  switch (type()) {
  case nullValue:
  case intValue:
  case uintValue:
  case realValue:
  case booleanValue:
    break;
  case stringValue:
    if (isAllocated())
      releasePrefixedStringValue(value_.string_);
    break;
  case arrayValue:
  case objectValue:
    delete value_.map_;
    break;
  default:
    JSON_ASSERT_UNREACHABLE;
  }
}

/*!
Copies metadata (comments and position information) from another Value object to this one, used internally for maintaining metadata during Value operations.
*/
void Value::dupMeta(const Value& other) {
  comments_ = other.comments_;
  start_ = other.start_;
  limit_ = other.limit_;
}

/*!
Ensures the Value is an object, then locates or creates a member with the given key.
If the key exists, returns a reference to its associated Value; otherwise, inserts a new null Value and returns a reference to it.
*/
Value& Value::resolveReference(const char* key) {
  JSON_ASSERT_MESSAGE(
      type() == nullValue || type() == objectValue,
      "in Json::Value::resolveReference(): requires objectValue");
  if (type() == nullValue)
    *this = Value(objectValue);
  CZString actualKey(key, static_cast<unsigned>(strlen(key)),
                     CZString::noDuplication);
  auto it = value_.map_->lower_bound(actualKey);
  if (it != value_.map_->end() && (*it).first == actualKey)
    return (*it).second;

  ObjectValues::value_type defaultValue(actualKey, nullSingleton());
  it = value_.map_->insert(it, defaultValue);
  Value& value = (*it).second;
  return value;
}

/*!
Ensures the Value is an object, then finds or creates a member with the given key.
If the member doesn't exist, it inserts a new null Value and returns a reference to it.
Uses lower_bound for efficient lookup in the internal map.
*/
Value& Value::resolveReference(char const* key, char const* end) {
  JSON_ASSERT_MESSAGE(
      type() == nullValue || type() == objectValue,
      "in Json::Value::resolveReference(key, end): requires objectValue");
  if (type() == nullValue)
    *this = Value(objectValue);
  CZString actualKey(key, static_cast<unsigned>(end - key),
                     CZString::duplicateOnCopy);
  auto it = value_.map_->lower_bound(actualKey);
  if (it != value_.map_->end() && (*it).first == actualKey)
    return (*it).second;

  ObjectValues::value_type defaultValue(actualKey, nullSingleton());
  it = value_.map_->insert(it, defaultValue);
  Value& value = (*it).second;
  return value;
}

/*!
Retrieves the value at the specified array index, returning the default value if the index is out of bounds or the element is null.
Uses the array access operator internally and compares the result against a null singleton for validation.
*/
Value Value::get(ArrayIndex index, const Value& defaultValue) const {
  const Value* value = &((*this)[index]);
  return value == &nullSingleton() ? defaultValue : *value;
}

/*!
Checks if the given index is within the bounds of the current JSON array by comparing it to the array's size.
*/
bool Value::isValidIndex(ArrayIndex index) const { return index < size(); }

/*!
Searches for a key within the current JSON object using the provided character range.
Returns a pointer to the associated Value if found, or nullptr if the key is not present or if the current value is not an object.
*/
Value const* Value::find(char const* begin, char const* end) const {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == objectValue,
                      "in Json::Value::find(begin, end): requires "
                      "objectValue or nullValue");
  if (type() == nullValue)
    return nullptr;
  CZString actualKey(begin, static_cast<unsigned>(end - begin),
                     CZString::noDuplication);
  ObjectValues::const_iterator it = value_.map_->find(actualKey);
  if (it == value_.map_->end())
    return nullptr;
  return &(*it).second;
}
/*!
Delegates the search operation to an overloaded version of the function, passing the key's data and length as separate arguments for efficient lookup in the JSON structure.
*/
Value const* Value::find(const String& key) const {
  return find(key.data(), key.data() + key.length());
}
/*!
Retrieves or creates a nested Value object within the current object, converting it to an objectValue if null.
Resolves the specified path and returns a pointer to the resulting Value.
*/
Value* Value::demand(char const* begin, char const* end) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == objectValue,
                      "in Json::Value::demand(begin, end): requires "
                      "objectValue or nullValue");
  return &resolveReference(begin, end);
}
const Value& Value::operator[](const char* key) const {
  Value const* found = find(key, key + strlen(key));
  if (!found)
    return nullSingleton();
  return *found;
}
Value const& Value::operator[](const String& key) const {
  Value const* found = find(key);
  if (!found)
    return nullSingleton();
  return *found;
}

Value& Value::operator[](const char* key) {
  return resolveReference(key, key + strlen(key));
}

Value& Value::operator[](const String& key) {
  return resolveReference(key.data(), key.data() + key.length());
}

Value& Value::operator[](const StaticString& key) {
  return resolveReference(key.c_str());
}

/*!
Appends the given value to the end of the array, creating an empty array first if necessary.
Returns a reference to the modified Value object.
*/
Value& Value::append(const Value& value) { return append(Value(value)); }

/*!
Appends the given value to the end of the array, converting a null value to an empty array if necessary.
Utilizes move semantics for efficient insertion and returns a reference to the newly appended value.
*/
Value& Value::append(Value&& value) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == arrayValue,
                      "in Json::Value::append: requires arrayValue");
  if (type() == nullValue) {
    *this = Value(arrayValue);
  }
  return this->value_.map_->emplace(size(), std::move(value)).first->second;
}

/*!
Inserts a copy of the provided value at the specified index in the array, shifting existing elements to make room.
Delegates the insertion operation to an overloaded version of the function.
*/
bool Value::insert(ArrayIndex index, const Value& newValue) {
  return insert(index, Value(newValue));
}

/*!
Inserts a value at the specified index in the array, shifting existing elements to make room.
Checks if the index is valid, moves existing elements, and places the new value at the specified position.
*/
bool Value::insert(ArrayIndex index, Value&& newValue) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == arrayValue,
                      "in Json::Value::insert: requires arrayValue");
  ArrayIndex length = size();
  if (index > length) {
    return false;
  }
  for (ArrayIndex i = length; i > index; i--) {
    (*this)[i] = std::move((*this)[i - 1]);
  }
  (*this)[index] = std::move(newValue);
  return true;
}

/*!
Searches for a value using the provided key range and returns the found value or the default value if not found.
Utilizes the `find` method internally to locate the requested key.
*/
Value Value::get(char const* begin, char const* end,
                 Value const& defaultValue) const {
  Value const* found = find(begin, end);
  return !found ? defaultValue : *found;
}
/*!
Delegates the key lookup to an overloaded version of the function, passing the key as a null-terminated string along with the default value.
Uses strlen to determine the key length.
*/
Value Value::get(char const* key, Value const& defaultValue) const {
  return get(key, key + strlen(key), defaultValue);
}
/*!
Retrieves a value from the JSON object using a string key, delegating to an internal overload.
Returns the associated value if found, otherwise the provided default value.
*/
Value Value::get(String const& key, Value const& defaultValue) const {
  return get(key.data(), key.data() + key.length(), defaultValue);
}

/*!
Removes a member from the JSON object if it exists, using the provided string range to identify the member.
Returns the removed member through the optional pointer if successful, and updates the internal map to reflect the removal.
*/
bool Value::removeMember(const char* begin, const char* end, Value* removed) {
  if (type() != objectValue) {
    return false;
  }
  CZString actualKey(begin, static_cast<unsigned>(end - begin),
                     CZString::noDuplication);
  auto it = value_.map_->find(actualKey);
  if (it == value_.map_->end())
    return false;
  if (removed)
    *removed = std::move(it->second);
  value_.map_->erase(it);
  return true;
}
/*!
Delegates the removal of a member from the JSON object to an overloaded version of the function.
Uses the provided key and calculates its length to determine the range for removal.
*/
bool Value::removeMember(const char* key, Value* removed) {
  return removeMember(key, key + strlen(key), removed);
}
/*!
Delegates the removal of a member from the JSON object to an overloaded version of the function.
Uses the provided key's data and length to perform the removal operation, optionally storing the removed value if a pointer is provided.
*/
bool Value::removeMember(String const& key, Value* removed) {
  return removeMember(key.data(), key.data() + key.length(), removed);
}
/*!
Removes the specified member from the JSON object if it exists.
Performs no action if the Value is not an object or is null.
Uses a CZString for efficient key comparison and map lookup.
*/
void Value::removeMember(const char* key) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == objectValue,
                      "in Json::Value::removeMember(): requires objectValue");
  if (type() == nullValue)
    return;

  CZString actualKey(key, unsigned(strlen(key)), CZString::noDuplication);
  value_.map_->erase(actualKey);
}
/*!
Delegates the removal of a member from the JSON object to the main implementation by converting the string key to a C-style string.
*/
void Value::removeMember(const String& key) { removeMember(key.c_str()); }

/*!
Removes an element from the array at the specified index, shifting subsequent elements to fill the gap.
If a valid pointer is provided, stores the removed element.
Returns true if successful, false if the index is invalid or the Value is not an array.
*/
bool Value::removeIndex(ArrayIndex index, Value* removed) {
  if (type() != arrayValue) {
    return false;
  }
  CZString key(index);
  auto it = value_.map_->find(key);
  if (it == value_.map_->end()) {
    return false;
  }
  if (removed)
    *removed = std::move(it->second);
  ArrayIndex oldSize = size();
  for (ArrayIndex i = index; i < (oldSize - 1); ++i) {
    CZString keey(i);
    (*value_.map_)[keey] = (*this)[i + 1];
  }
  CZString keyLast(oldSize - 1);
  auto itLast = value_.map_->find(keyLast);
  value_.map_->erase(itLast);
  return true;
}

/*!
Checks for the existence of a member within the specified character range by searching for a matching value.
Returns true if a member is found, false otherwise.
*/
bool Value::isMember(char const* begin, char const* end) const {
  Value const* value = find(begin, end);
  return nullptr != value;
}
/*!
Delegates to an overloaded version of isMember, passing the key as a string range from the start of the key to its null terminator.
This approach allows for efficient key comparison without creating temporary string objects.
*/
bool Value::isMember(char const* key) const {
  return isMember(key, key + strlen(key));
}
/*!
Delegates the key existence check to an overloaded version of the function, converting the input string to a pair of pointers representing the start and end of the key data.
*/
bool Value::isMember(String const& key) const {
  return isMember(key.data(), key.data() + key.length());
}

/*!
Retrieves the names of all members in a JSON object, returning them as a vector of strings.
Asserts that the value is either null or an object, returning an empty vector for null values.
*/
Value::Members Value::getMemberNames() const {
  JSON_ASSERT_MESSAGE(
      type() == nullValue || type() == objectValue,
      "in Json::Value::getMemberNames(), value must be objectValue");
  if (type() == nullValue)
    return Value::Members();
  Members members;
  members.reserve(value_.map_->size());
  ObjectValues::const_iterator it = value_.map_->begin();
  ObjectValues::const_iterator itEnd = value_.map_->end();
  for (; it != itEnd; ++it) {
    members.push_back(String((*it).first.data(), (*it).first.length()));
  }
  return members;
}

/*!
\brief Checks if a floating-point number is integral.

Determines whether a given double value can be represented as an integer without loss of precision.
This function is used for type-checking in JSON value handling to ensure accurate type conversions and validations.

\param d The double value to be checked for integrality.

\return True if the value is integral (can be represented as an integer without loss of precision), false otherwise.
*/
static bool IsIntegral(double d) {
  double integral_part;
  return modf(d, &integral_part) == 0.0;
}

/*!
Checks if the current Value object represents a JSON null value by comparing its type to nullValue.
*/
bool Value::isNull() const { return type() == nullValue; }

/*!
Checks if the current Value object represents a boolean by comparing its type to booleanValue.
*/
bool Value::isBool() const { return type() == booleanValue; }

/*!
Checks if the value can be represented as a 32-bit signed integer by examining its type and value.
For integer types, ensures the value is within the valid range, while for floating-point types, verifies both range and integrality.
*/
bool Value::isInt() const {
  switch (type()) {
  case intValue:
#if defined(JSON_HAS_INT64)
    return value_.int_ >= minInt && value_.int_ <= maxInt;
#else
    return true;
#endif
  case uintValue:
    return value_.uint_ <= UInt(maxInt);
  case realValue:
    return value_.real_ >= minInt && value_.real_ <= maxInt &&
           IsIntegral(value_.real_);
  default:
    break;
  }
  return false;
}

/*!
Checks if the value can be represented as an unsigned integer by examining its type and range.
Handles different numeric types, including integers and floating-point numbers, considering platform-specific integer size limitations.
*/
bool Value::isUInt() const {
  switch (type()) {
  case intValue:
#if defined(JSON_HAS_INT64)
    return value_.int_ >= 0 && LargestUInt(value_.int_) <= LargestUInt(maxUInt);
#else
    return value_.int_ >= 0;
#endif
  case uintValue:
#if defined(JSON_HAS_INT64)
    return value_.uint_ <= maxUInt;
#else
    return true;
#endif
  case realValue:
    return value_.real_ >= 0 && value_.real_ <= maxUInt &&
           IsIntegral(value_.real_);
  default:
    break;
  }
  return false;
}

/*!
Checks if the value can be represented as a 64-bit signed integer by examining its type and range.
For integer and unsigned integer types, it verifies if the value falls within the int64 bounds.
For floating-point types, it checks if the value is integral and within the int64 range.
*/
bool Value::isInt64() const {
#if defined(JSON_HAS_INT64)
  switch (type()) {
  case intValue:
    return true;
  case uintValue:
    return value_.uint_ <= UInt64(maxInt64);
  case realValue:
    return value_.real_ > double(minInt64) && value_.real_ < double(maxInt64) &&
           IsIntegral(value_.real_);
  default:
    break;
  }
#endif
  return false;
}

/*!
Checks if the value can be represented as an unsigned 64-bit integer.
Evaluates different value types, including integers, unsigned integers, and real numbers, to determine if they fall within the valid range for an unsigned 64-bit integer.
*/
bool Value::isUInt64() const {
#if defined(JSON_HAS_INT64)
  switch (type()) {
  case intValue:
    return value_.int_ >= 0;
  case uintValue:
    return true;
  case realValue:
    return value_.real_ >= 0 && value_.real_ < maxUInt64AsDouble &&
           IsIntegral(value_.real_);
  default:
    break;
  }
#endif
  return false;
}

/*!
Determines if the value is an integral number by checking its type and, for real values, evaluating whether it can be accurately represented as an integer.
Utilizes type-specific checks and the IsIntegral function for floating-point values.
*/
bool Value::isIntegral() const {
  switch (type()) {
  case intValue:
  case uintValue:
    return true;
  case realValue:
#if defined(JSON_HAS_INT64)
    return value_.real_ > double(minInt64) &&
           value_.real_ < maxUInt64AsDouble && IsIntegral(value_.real_);
#else
    return value_.real_ >= minInt && value_.real_ <= maxUInt &&
           IsIntegral(value_.real_);
#endif
  default:
    break;
  }
  return false;
}

/*!
Checks if the value is a double or can be accurately represented as one, including integer types that can be converted to double without precision loss.
Returns true for integer, unsigned integer, and real value types.
*/
bool Value::isDouble() const {
  return type() == intValue || type() == uintValue || type() == realValue;
}

/*!
Checks if the value represents a numeric type by determining if it is a double.
Delegates the check to the isDouble() method.
*/
bool Value::isNumeric() const { return isDouble(); }

/*!
Checks if the current value is of string type by comparing its internal type representation to the string value type.
*/
bool Value::isString() const { return type() == stringValue; }

/*!
Checks if the current JSON value represents an array by comparing its type to arrayValue.
*/
bool Value::isArray() const { return type() == arrayValue; }

/*!
Checks if the current value represents a JSON object by comparing its type to `objectValue`.
Returns true if it is an object, false otherwise.
*/
bool Value::isObject() const { return type() == objectValue; }

/*!
Performs a deep copy of the provided Comments object, creating a new instance with its own independent copy of the comment data using the cloneUnique function.
*/
Value::Comments::Comments(const Comments& that)
    : ptr_{cloneUnique(that.ptr_)} {}

/*!
Moves the comment data from the source object to the newly constructed object, transferring ownership of the resources.
Leaves the source object in a valid but unspecified state.
*/
Value::Comments::Comments(Comments&& that) noexcept
    : ptr_{std::move(that.ptr_)} {}

Value::Comments& Value::Comments::operator=(const Comments& that) {
  ptr_ = cloneUnique(that.ptr_);
  return *this;
}

Value::Comments& Value::Comments::operator=(Comments&& that) noexcept {
  ptr_ = std::move(that.ptr_);
  return *this;
}

/*!
Checks for the existence of a non-empty comment in the specified placement slot.
Returns true if a comment is present, false otherwise.
*/
bool Value::Comments::has(CommentPlacement slot) const {
  return ptr_ && !(*ptr_)[slot].empty();
}

/*!
Retrieves the comment associated with the specified placement slot.
Returns an empty string if no comment exists or if the internal pointer is null.
*/
String Value::Comments::get(CommentPlacement slot) const {
  if (!ptr_)
    return {};
  return (*ptr_)[slot];
}

/*!
Sets the comment for the specified placement slot if valid.
Creates internal storage if necessary and moves the provided comment string into the appropriate slot.
*/
void Value::Comments::set(CommentPlacement slot, String comment) {
  if (slot >= CommentPlacement::numberOfCommentPlacement)
    return;
  if (!ptr_)
    ptr_ = std::unique_ptr<Array>(new Array());
  (*ptr_)[slot] = std::move(comment);
}

/*!
Sets the comment for the JSON value at the specified placement.
Removes any trailing newline from the comment, ensures it starts with a forward slash, and uses the internal Comments structure to store the comment.
*/
void Value::setComment(String comment, CommentPlacement placement) {
  if (!comment.empty() && (comment.back() == '\n')) {
    comment.pop_back();
  }
  JSON_ASSERT_MESSAGE(
      comment.empty() || comment[0] == '/',
      "in Json::Value::setComment(): Comments must start with /");
  comments_.set(placement, std::move(comment));
}

/*!
Checks for the presence of a comment in the specified placement by delegating to the internal comments_ object.
Returns true if a comment exists in the given placement, false otherwise.
*/
bool Value::hasComment(CommentPlacement placement) const {
  return comments_.has(placement);
}

/*!
Retrieves the comment associated with the specified placement from the JSON value.
Delegates the actual retrieval to the internal comments_ object.
*/
String Value::getComment(CommentPlacement placement) const {
  return comments_.get(placement);
}

/*!
Sets the starting offset position of the JSON value within the original parsed input.
This information is used for error reporting, formatting, and structure preservation.
*/
void Value::setOffsetStart(ptrdiff_t start) { start_ = start; }

/*!
Sets the ending offset of the JSON value within the parsed document.
Updates the internal limit_ member with the provided value.
*/
void Value::setOffsetLimit(ptrdiff_t limit) { limit_ = limit; }

/*!
Returns the starting position of the JSON value in the input stream.
This offset is stored in the `start_` member variable.
*/
ptrdiff_t Value::getOffsetStart() const { return start_; }

/*!
Returns the end position of the JSON value in the input stream by accessing the `limit_` member variable.
This offset is used to define the span of a JSON value for parsing and error reporting purposes.
*/
ptrdiff_t Value::getOffsetLimit() const { return limit_; }

/*!
Generates a formatted string representation of the JSON value, including any associated comments.
Utilizes a StreamWriterBuilder for customizable output formatting and appends newline characters for improved readability.
*/
String Value::toStyledString() const {
  StreamWriterBuilder builder;

  String out = this->hasComment(commentBefore) ? "\n" : "";
  out += Json::writeString(builder, *this);
  out += '\n';

  return out;
}

/*!
Returns a const_iterator to the beginning of the value if it's an array or object.
For other types, returns an empty iterator.
The function checks the value type and initializes the iterator accordingly.
*/
Value::const_iterator Value::begin() const {
  switch (type()) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return const_iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return {};
}

/*!
Returns an iterator to the end of the array or object if the value is of those types.
For other types, returns an empty iterator.
Uses the type() method to determine the appropriate action based on the value's type.
*/
Value::const_iterator Value::end() const {
  switch (type()) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return const_iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return {};
}

/*!
Returns an iterator to the beginning of the array or object value.
For other value types, returns an empty iterator, enabling iteration over elements when applicable.
*/
Value::iterator Value::begin() {
  switch (type()) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return iterator(value_.map_->begin());
    break;
  default:
    break;
  }
  return iterator();
}

/*!
Returns an iterator to the end of the array or object, or an empty iterator for other value types.
Utilizes the internal map structure for arrays and objects, falling back to an empty iterator for non-container types.
*/
Value::iterator Value::end() {
  switch (type()) {
  case arrayValue:
  case objectValue:
    if (value_.map_)
      return iterator(value_.map_->end());
    break;
  default:
    break;
  }
  return iterator();
}

PathArgument::PathArgument() = default;

/*!
Initializes the PathArgument with an array index, setting the kind to kindIndex.
This constructor is used for creating path components that represent specific positions in JSON arrays.
*/
PathArgument::PathArgument(ArrayIndex index)
    : index_(index), kind_(kindIndex) {}

/*!
Initializes the PathArgument with a string key, setting its kind to 'kindKey' for navigation through JSON object structures.
*/
PathArgument::PathArgument(const char* key) : key_(key), kind_(kindKey) {}

/*!
Initializes the PathArgument with a key for JSON object navigation.
Moves the provided key into the internal storage and sets the kind to 'kindKey'.
*/
PathArgument::PathArgument(String key) : key_(std::move(key)), kind_(kindKey) {}

/*!
Constructs a Path object by combining a string representation of the base path with up to five optional PathArguments.
Reserves space for the arguments, adds them to a vector, and then calls makePath to parse and construct the complete path structure.
*/
Path::Path(const String& path, const PathArgument& a1, const PathArgument& a2,
           const PathArgument& a3, const PathArgument& a4,
           const PathArgument& a5) {
  InArgs in;
  in.reserve(5);
  in.push_back(&a1);
  in.push_back(&a2);
  in.push_back(&a3);
  in.push_back(&a4);
  in.push_back(&a5);
  makePath(path, in);
}

/*!
Parses a string representation of a JSON path, constructing a structured path by interpreting special characters for array indices and object keys.
Incorporates dynamic path components using placeholders and additional arguments, building an internal representation of the path for subsequent JSON traversal operations.
*/
void Path::makePath(const String& path, const InArgs& in) {
  const char* current = path.c_str();
  const char* end = current + path.length();
  auto itInArg = in.begin();
  while (current != end) {
    if (*current == '[') {
      ++current;
      if (*current == '%')
        addPathInArg(path, in, itInArg, PathArgument::kindIndex);
      else {
        ArrayIndex index = 0;
        for (; current != end && *current >= '0' && *current <= '9'; ++current)
          index = index * 10 + ArrayIndex(*current - '0');
        args_.push_back(index);
      }
      if (current == end || *++current != ']')
        invalidPath(path, int(current - path.c_str()));
    } else if (*current == '%') {
      addPathInArg(path, in, itInArg, PathArgument::kindKey);
      ++current;
    } else if (*current == '.' || *current == ']') {
      ++current;
    } else {
      const char* beginName = current;
      while (current != end && !strchr("[.", *current))
        ++current;
      args_.push_back(String(beginName, current));
    }
  }
}

/*!
Validates and appends a dynamic path component to the path's argument list if it matches the expected kind.
Advances the input argument iterator upon successful addition.
*/
void Path::addPathInArg(const String&, const InArgs& in,
                        InArgs::const_iterator& itInArg,
                        PathArgument::Kind kind) {
  if (itInArg == in.end()) {
  } else if ((*itInArg)->kind_ != kind) {
  } else {
    args_.push_back(**itInArg++);
  }
}

/*!
Serves as a placeholder for error handling when an invalid JSON path is encountered.
Currently implemented as a no-op, but may be extended for custom error reporting or exception handling in the future.
*/
void Path::invalidPath(const String&, int) {}

/*!
Traverses the JSON structure specified by the path, starting from the given root value.
Iterates through path arguments, handling both array indices and object keys, and returns the resolved value or a null value if the path is invalid.
*/
const Value& Path::resolve(const Value& root) const {
  const Value* node = &root;
  for (const auto& arg : args_) {
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray() || !node->isValidIndex(arg.index_)) {
        return Value::nullSingleton();
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject()) {
        return Value::nullSingleton();
      }
      node = &((*node)[arg.key_]);
      if (node == &Value::nullSingleton()) {
        return Value::nullSingleton();
      }
    }
  }
  return *node;
}

/*!
Traverses the JSON structure following the specified path, returning the value at the end of the path if it exists.
Handles both array indices and object keys, returning the default value if any part of the path is invalid or non-existent.
*/
Value Path::resolve(const Value& root, const Value& defaultValue) const {
  const Value* node = &root;
  for (const auto& arg : args_) {
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray() || !node->isValidIndex(arg.index_))
        return defaultValue;
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject())
        return defaultValue;
      node = &((*node)[arg.key_]);
      if (node == &Value::nullSingleton())
        return defaultValue;
    }
  }
  return *node;
}

/*!
Traverses the JSON structure using stored path arguments, creating new objects or arrays as needed.
Returns a reference to the JSON value at the end of the path, either newly created or pre-existing.
*/
Value& Path::make(Value& root) const {
  Value* node = &root;
  for (const auto& arg : args_) {
    if (arg.kind_ == PathArgument::kindIndex) {
      if (!node->isArray()) {
      }
      node = &((*node)[arg.index_]);
    } else if (arg.kind_ == PathArgument::kindKey) {
      if (!node->isObject()) {
      }
      node = &((*node)[arg.key_]);
    }
  }
  return *node;
}

} // namespace Json
