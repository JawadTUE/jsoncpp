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
static inline void releasePrefixedStringValue(char* value) { free(value); }
static inline void releaseStringValue(char* value, unsigned) { free(value); }
#endif

} // namespace Json

#if !defined(JSON_IS_AMALGAMATION)

#include "json_valueiterator.inl"
#endif

namespace Json {

#if JSON_USE_EXCEPTION
Exception::Exception(String msg) : msg_(std::move(msg)) {}
Exception::~Exception() noexcept = default;
char const* Exception::what() const noexcept { return msg_.c_str(); }
RuntimeError::RuntimeError(String const& msg) : Exception(msg) {}
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

Value::Value(Int value) {
  initBasic(intValue);
  value_.int_ = value;
}

Value::Value(UInt value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#if defined(JSON_HAS_INT64)
Value::Value(Int64 value) {
  initBasic(intValue);
  value_.int_ = value;
}
Value::Value(UInt64 value) {
  initBasic(uintValue);
  value_.uint_ = value;
}
#endif

Value::Value(double value) {
  initBasic(realValue);
  value_.real_ = value;
}

Value::Value(const char* value) {
  initBasic(stringValue, true);
  JSON_ASSERT_MESSAGE(value != nullptr,
                      "Null Value Passed to Value Constructor");
  value_.string_ = duplicateAndPrefixStringValue(
      value, static_cast<unsigned>(strlen(value)));
}

Value::Value(const char* begin, const char* end) {
  initBasic(stringValue, true);
  value_.string_ =
      duplicateAndPrefixStringValue(begin, static_cast<unsigned>(end - begin));
}

Value::Value(const String& value) {
  initBasic(stringValue, true);
  value_.string_ = duplicateAndPrefixStringValue(
      value.data(), static_cast<unsigned>(value.length()));
}

Value::Value(const StaticString& value) {
  initBasic(stringValue);
  value_.string_ = const_cast<char*>(value.c_str());
}

Value::Value(bool value) {
  initBasic(booleanValue);
  value_.bool_ = value;
}

Value::Value(const Value& other) {
  dupPayload(other);
  dupMeta(other);
}

Value::Value(Value&& other) noexcept {
  initBasic(nullValue);
  swap(other);
}

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

void Value::swapPayload(Value& other) {
  std::swap(bits_, other.bits_);
  std::swap(value_, other.value_);
}

void Value::copyPayload(const Value& other) {
  releasePayload();
  dupPayload(other);
}

void Value::swap(Value& other) {
  swapPayload(other);
  std::swap(comments_, other.comments_);
  std::swap(start_, other.start_);
  std::swap(limit_, other.limit_);
}

void Value::copy(const Value& other) {
  copyPayload(other);
  dupMeta(other);
}

ValueType Value::type() const {
  return static_cast<ValueType>(bits_.value_type_);
}

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

LargestInt Value::asLargestInt() const {
#if defined(JSON_NO_INT64)
  return asInt();
#else
  return asInt64();
#endif
}

LargestUInt Value::asLargestUInt() const {
#if defined(JSON_NO_INT64)
  return asUInt();
#else
  return asUInt64();
#endif
}

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

bool Value::empty() const {
  if (isNull() || isArray() || isObject())
    return size() == 0U;
  return false;
}

Value::operator bool() const { return !isNull(); }

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

void Value::initBasic(ValueType type, bool allocated) {
  setType(type);
  setIsAllocated(allocated);
  comments_ = Comments{};
  start_ = 0;
  limit_ = 0;
}

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

void Value::dupMeta(const Value& other) {
  comments_ = other.comments_;
  start_ = other.start_;
  limit_ = other.limit_;
}

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

Value Value::get(ArrayIndex index, const Value& defaultValue) const {
  const Value* value = &((*this)[index]);
  return value == &nullSingleton() ? defaultValue : *value;
}

bool Value::isValidIndex(ArrayIndex index) const { return index < size(); }

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
Value const* Value::find(const String& key) const {
  return find(key.data(), key.data() + key.length());
}
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

Value& Value::append(const Value& value) { return append(Value(value)); }

Value& Value::append(Value&& value) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == arrayValue,
                      "in Json::Value::append: requires arrayValue");
  if (type() == nullValue) {
    *this = Value(arrayValue);
  }
  return this->value_.map_->emplace(size(), std::move(value)).first->second;
}

bool Value::insert(ArrayIndex index, const Value& newValue) {
  return insert(index, Value(newValue));
}

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

Value Value::get(char const* begin, char const* end,
                 Value const& defaultValue) const {
  Value const* found = find(begin, end);
  return !found ? defaultValue : *found;
}
Value Value::get(char const* key, Value const& defaultValue) const {
  return get(key, key + strlen(key), defaultValue);
}
Value Value::get(String const& key, Value const& defaultValue) const {
  return get(key.data(), key.data() + key.length(), defaultValue);
}

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
bool Value::removeMember(const char* key, Value* removed) {
  return removeMember(key, key + strlen(key), removed);
}
bool Value::removeMember(String const& key, Value* removed) {
  return removeMember(key.data(), key.data() + key.length(), removed);
}
void Value::removeMember(const char* key) {
  JSON_ASSERT_MESSAGE(type() == nullValue || type() == objectValue,
                      "in Json::Value::removeMember(): requires objectValue");
  if (type() == nullValue)
    return;

  CZString actualKey(key, unsigned(strlen(key)), CZString::noDuplication);
  value_.map_->erase(actualKey);
}
void Value::removeMember(const String& key) { removeMember(key.c_str()); }

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

bool Value::isMember(char const* begin, char const* end) const {
  Value const* value = find(begin, end);
  return nullptr != value;
}
bool Value::isMember(char const* key) const {
  return isMember(key, key + strlen(key));
}
bool Value::isMember(String const& key) const {
  return isMember(key.data(), key.data() + key.length());
}

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

static bool IsIntegral(double d) {
  double integral_part;
  return modf(d, &integral_part) == 0.0;
}

bool Value::isNull() const { return type() == nullValue; }

bool Value::isBool() const { return type() == booleanValue; }

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

bool Value::isDouble() const {
  return type() == intValue || type() == uintValue || type() == realValue;
}

bool Value::isNumeric() const { return isDouble(); }

bool Value::isString() const { return type() == stringValue; }

bool Value::isArray() const { return type() == arrayValue; }

bool Value::isObject() const { return type() == objectValue; }

Value::Comments::Comments(const Comments& that)
    : ptr_{cloneUnique(that.ptr_)} {}

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

bool Value::Comments::has(CommentPlacement slot) const {
  return ptr_ && !(*ptr_)[slot].empty();
}

String Value::Comments::get(CommentPlacement slot) const {
  if (!ptr_)
    return {};
  return (*ptr_)[slot];
}

void Value::Comments::set(CommentPlacement slot, String comment) {
  if (slot >= CommentPlacement::numberOfCommentPlacement)
    return;
  if (!ptr_)
    ptr_ = std::unique_ptr<Array>(new Array());
  (*ptr_)[slot] = std::move(comment);
}

void Value::setComment(String comment, CommentPlacement placement) {
  if (!comment.empty() && (comment.back() == '\n')) {
    comment.pop_back();
  }
  JSON_ASSERT_MESSAGE(
      comment.empty() || comment[0] == '/',
      "in Json::Value::setComment(): Comments must start with /");
  comments_.set(placement, std::move(comment));
}

bool Value::hasComment(CommentPlacement placement) const {
  return comments_.has(placement);
}

String Value::getComment(CommentPlacement placement) const {
  return comments_.get(placement);
}

void Value::setOffsetStart(ptrdiff_t start) { start_ = start; }

void Value::setOffsetLimit(ptrdiff_t limit) { limit_ = limit; }

ptrdiff_t Value::getOffsetStart() const { return start_; }

ptrdiff_t Value::getOffsetLimit() const { return limit_; }

String Value::toStyledString() const {
  StreamWriterBuilder builder;

  String out = this->hasComment(commentBefore) ? "\n" : "";
  out += Json::writeString(builder, *this);
  out += '\n';

  return out;
}

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

PathArgument::PathArgument(ArrayIndex index)
    : index_(index), kind_(kindIndex) {}

PathArgument::PathArgument(const char* key) : key_(key), kind_(kindKey) {}

PathArgument::PathArgument(String key) : key_(std::move(key)), kind_(kindKey) {}

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

void Path::addPathInArg(const String&, const InArgs& in,
                        InArgs::const_iterator& itInArg,
                        PathArgument::Kind kind) {
  if (itInArg == in.end()) {
  } else if ((*itInArg)->kind_ != kind) {
  } else {
    args_.push_back(**itInArg++);
  }
}

void Path::invalidPath(const String&, int) {}

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
