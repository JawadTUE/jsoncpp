#ifndef JSON_VALUE_H_INCLUDED
#define JSON_VALUE_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "forwards.h"
#endif

#if !defined(JSONCPP_NORETURN)
#if defined(_MSC_VER) && _MSC_VER == 1800
#define JSONCPP_NORETURN __declspec(noreturn)
#else
#define JSONCPP_NORETURN [[noreturn]]
#endif
#endif

#if !defined(JSONCPP_TEMPLATE_DELETE)
#if defined(__clang__) && defined(__apple_build_version__)
#if __apple_build_version__ <= 8000042
#define JSONCPP_TEMPLATE_DELETE
#endif
#elif defined(__clang__)
#if __clang_major__ == 3 && __clang_minor__ <= 8
#define JSONCPP_TEMPLATE_DELETE
#endif
#endif
#if !defined(JSONCPP_TEMPLATE_DELETE)
#define JSONCPP_TEMPLATE_DELETE = delete
#endif
#endif

#include <array>
#include <exception>
#include <map>
#include <memory>
#include <string>
#include <vector>

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251 4275)
#endif

#pragma pack(push)
#pragma pack()

namespace Json {

#if JSON_USE_EXCEPTION
/*!
\class Exception
\brief Represents a custom exception for JSON-related errors.

Encapsulates error information specific to JSON operations, providing a mechanism for precise error handling and reporting within the library.
Allows for the creation of exceptions with custom error messages and offers a method to retrieve the error description.
*/
class JSON_API Exception : public std::exception {
public:
  /*!
  \brief Constructs a JSON exception with a custom error message.
  
  Creates a new Json::Exception object with the specified error message.
  The message is moved into the exception object, allowing for efficient handling of error information.
  
  \param msg The error message describing the JSON-related exception.
  */
  Exception(String msg);
  ~Exception() noexcept override;
  /*!
  \brief Retrieves the error message.
  
  Provides access to the error message associated with this exception.
  Overrides the standard what() method from std::exception to return a C-style string containing the specific error description for this JSON exception.
  
  \return A pointer to a null-terminated string containing the error message.
  */
  char const* what() const noexcept override;

protected:
  String msg_;
};

/*!
\class RuntimeError
\brief Represents a runtime error specific to JSON operations.

Encapsulates runtime errors that occur during JSON processing.
Provides a mechanism for throwing and handling exceptions with custom error messages, allowing for precise error reporting and management within the JSON library.
*/
class JSON_API RuntimeError : public Exception {
public:
  /*!
  \brief Constructs a RuntimeError exception.
  
  Creates a new RuntimeError instance with a custom error message.
  This constructor is used to generate specific runtime error exceptions during JSON processing operations.
  
  \param msg The error message describing the specific runtime error encountered.
  */
  RuntimeError(String const& msg);
};

/*!
\class LogicError
\brief Represents a logical error in JSON processing.

Extends the Exception class to handle specific logical errors that occur during JSON operations.
Provides a way to create custom error messages for precise error reporting and maintains consistency with the library's error handling mechanism.
*/
class JSON_API LogicError : public Exception {
public:
  /*!
  \brief Constructs a LogicError exception.
  
  Creates a new LogicError exception with a custom error message.
  This constructor allows for specific error details to be passed when a logical error occurs during JSON processing.
  
  \param msg The custom error message describing the logical error.
  */
  LogicError(String const& msg);
};
#endif

JSONCPP_NORETURN void throwRuntimeError(String const& msg);
JSONCPP_NORETURN void throwLogicError(String const& msg);

enum ValueType {
  nullValue = 0,
  intValue,
  uintValue,
  realValue,
  stringValue,
  booleanValue,
  arrayValue,
  objectValue
};

enum CommentPlacement {
  commentBefore = 0,
  commentAfterOnSameLine,
  commentAfter,

  numberOfCommentPlacement
};

enum PrecisionType { significantDigits = 0, decimalPlaces };

/*!
\class StaticString
\brief Represents a static string wrapper for C-style strings.

Provides an efficient interface for storing and accessing immutable character arrays.
Designed for use with constant string literals in JSON processing, it offers implicit conversion to const char* and direct access to the underlying C-string.
Ensures that the wrapped string remains valid throughout the object's lifetime.
*/
class JSON_API StaticString {
public:
  /*!
  \brief Constructs a StaticString from a C-style string.
  
  Creates a StaticString object that wraps the provided C-style string.
  This constructor does not copy the string; it only stores a pointer to the input string.
  Ensure that the lifetime of the provided string exceeds that of the StaticString object.
  
  \param czstring Pointer to a null-terminated C-style string. Must remain valid for the lifetime of the StaticString object.
  */
  explicit StaticString(const char* czstring) : c_str_(czstring) {}

  operator const char*() const { return c_str_; }

  /*!
  \brief Retrieves the underlying C-style string.
  
  Provides access to the internal const char* representation of the static string.
  This method is essential for interoperability with C-style string functions and for initializing JSON values.
  
  \return A pointer to the null-terminated character array stored in this object.
  */
  const char* c_str() const { return c_str_; }

private:
  const char* c_str_;
};

/*!
\class Value
\brief Represents and manipulates JSON data structures.

Provides a versatile container for JSON data, supporting various types including numbers, strings, booleans, arrays, and objects.
Offers methods for accessing, modifying, and querying JSON elements, as well as type checking and conversion functions.
Includes features for handling comments and efficient memory management, enabling comprehensive JSON manipulation in C++ applications.
*/
class JSON_API Value {
  friend class ValueIteratorBase;

public:
  using Members = std::vector<String>;
  using iterator = ValueIterator;
  using const_iterator = ValueConstIterator;
  using UInt = Json::UInt;
  using Int = Json::Int;
#if defined(JSON_HAS_INT64)
  using UInt64 = Json::UInt64;
  using Int64 = Json::Int64;
#endif
  using LargestInt = Json::LargestInt;
  using LargestUInt = Json::LargestUInt;
  using ArrayIndex = Json::ArrayIndex;

  using value_type = std::string;

#if JSON_USE_NULLREF

  static const Value& null;
  static const Value& nullRef;
#endif

  /*!
  \brief Returns a reference to a static null JSON value.
  
  Provides access to a singleton null JSON value.
  This function is used as a sentinel to represent invalid or non-existent JSON elements during parsing and navigation, particularly in path resolution operations.
  
  \return A constant reference to a static null Json::Value object.
  */
  static Value const& nullSingleton();

  static constexpr LargestInt minLargestInt =
      LargestInt(~(LargestUInt(-1) / 2));

  static constexpr LargestInt maxLargestInt = LargestInt(LargestUInt(-1) / 2);

  static constexpr LargestUInt maxLargestUInt = LargestUInt(-1);

  static constexpr Int minInt = Int(~(UInt(-1) / 2));

  static constexpr Int maxInt = Int(UInt(-1) / 2);

  static constexpr UInt maxUInt = UInt(-1);

#if defined(JSON_HAS_INT64)

  static constexpr Int64 minInt64 = Int64(~(UInt64(-1) / 2));

  static constexpr Int64 maxInt64 = Int64(UInt64(-1) / 2);

  static constexpr UInt64 maxUInt64 = UInt64(-1);
#endif

  static constexpr UInt defaultRealPrecision = 17;

  static constexpr double maxUInt64AsDouble = 18446744073709551615.0;
#ifdef __NVCC__
public:
#else
private:
#endif
#ifndef JSONCPP_DOC_EXCLUDE_IMPLEMENTATION
  class CZString {
  public:
    enum DuplicationPolicy { noDuplication = 0, duplicate, duplicateOnCopy };
    CZString(ArrayIndex index);
    CZString(char const* str, unsigned length, DuplicationPolicy allocate);
    CZString(CZString const& other);
    CZString(CZString&& other) noexcept;
    ~CZString();
    CZString& operator=(const CZString& other);
    CZString& operator=(CZString&& other) noexcept;

    bool operator<(CZString const& other) const;
    bool operator==(CZString const& other) const;
    ArrayIndex index() const;

    char const* data() const;
    unsigned length() const;
    bool isStaticString() const;

  private:
    void swap(CZString& other);

    struct StringStorage {
      unsigned policy_ : 2;
      unsigned length_ : 30;
    };

    char const* cstr_;
    union {
      ArrayIndex index_;
      StringStorage storage_;
    };
  };

public:
  typedef std::map<CZString, Value> ObjectValues;
#endif

public:
  /*!
  \brief Constructs a JSON value of the specified type.
  
  Initializes a new Json::Value instance with the given ValueType.
  Sets default values based on the type: 0 for numbers, false for booleans, empty string for strings, and empty container for arrays and objects.
  
  \param type The ValueType enum specifying the desired JSON value type to initialize.
  */
  Value(ValueType type = nullValue);
  /*!
  \brief Constructs a JSON value from an integer.
  
  Creates a new Json::Value object initialized with the provided integer value.
  This constructor allows for easy creation of JSON integer values, which can be used in larger JSON structures or as standalone elements.
  
  \param value The integer value to initialize this JSON value with.
  */
  Value(Int value);
  /*!
  \brief Constructs a JSON value containing an unsigned integer.
  
  Creates a new Json::Value object initialized with the given unsigned integer.
  The internal type of the value is set to uintValue, allowing for efficient storage and manipulation of unsigned integer data within the JSON structure.
  
  \param value The unsigned integer to be stored in the JSON value.
  */
  Value(UInt value);
#if defined(JSON_HAS_INT64)
  /*!
  \brief Constructs a JSON value from a 64-bit integer.
  
  Creates a new Json::Value object initialized with the provided 64-bit integer.
  The resulting value will have an integer type and can be used in JSON operations or serialization.
  
  \param value The 64-bit integer value to initialize the JSON value with.
  */
  Value(Int64 value);
  /*!
  \brief Constructs a JSON value from an unsigned 64-bit integer.
  
  Creates a new Json::Value object initialized with the given unsigned 64-bit integer.
  The resulting value will be of type uintValue, representing an unsigned integer in the JSON structure.
  
  \param value The unsigned 64-bit integer value to initialize the JSON value with.
  */
  Value(UInt64 value);
#endif
  /*!
  \brief Constructs a JSON value containing a double-precision floating-point number.
  
  Creates a new Json::Value object initialized with the provided double value.
  This constructor allows for the direct creation of a JSON number from a C++ double, facilitating the representation of floating-point data in JSON format.
  
  \param value The double-precision floating-point number to be stored in the JSON value.
  */
  Value(double value);
  /*!
  \brief Constructs a Value object from a C-style string.
  
  Creates a new Value object of string type, initializing it with the provided C-style string.
  The constructor takes ownership of the string by creating a copy of it internally.
  
  \param value Pointer to a null-terminated string. Must not be null.
  */
  Value(const char* value);
  /*!
  \brief Constructs a JSON string value from a character range.
  
  Creates a new Json::Value object containing a string.
  The string is constructed from the characters in the range [begin, end).
  The constructor takes ownership of the string data by duplicating it internally.
  
  \param begin Pointer to the first character of the string.
  \param end Pointer to one past the last character of the string.
  */
  Value(const char* begin, const char* end);

  /*!
  \brief Constructs a Value object from a static string.
  
  Creates a new Value object initialized with a string value.
  The constructor takes ownership of the provided static string without copying its contents, improving performance for constant string literals.
  
  \param value A static string to initialize the Value object. The string's lifetime must exceed that of the Value object.
  */
  Value(const StaticString& value);
  /*!
  \brief Constructs a JSON string value.
  
  Creates a new Value object initialized with a string.
  The constructor takes ownership of the string data by creating a duplicate of the input string.
  
  \param value String value to initialize the JSON value with.
  */
  Value(const String& value);
  /*!
  \brief Constructs a boolean JSON value.
  
  Creates a new Json::Value instance initialized with a boolean value.
  This constructor allows for the direct creation of a boolean JSON element, which can be used in building more complex JSON structures.
  
  \param value The boolean value to initialize this Json::Value with.
  */
  Value(bool value);
  Value(std::nullptr_t ptr) = delete;
  /*!
  \brief Creates a copy of another Json::Value object.
  
  Performs a deep copy of the given Json::Value object, including its payload and metadata.
  This constructor ensures that the newly created object is an independent copy of the original, with its own allocated memory for dynamic data.
  
  \param other The Json::Value object to be copied.
  */
  Value(const Value& other);
  /*!
  \brief Constructs a Value by moving the contents of another Value.
  
  Creates a new Value object by efficiently transferring the contents of the provided Value.
  This move constructor leaves the source object in a valid but unspecified state.
  It's useful for optimizing performance when temporary Value objects are created and then moved.
  
  \param other The Value object to move from. Its contents will be transferred to the newly constructed object.
  */
  Value(Value&& other) noexcept;
  /*!
  \brief Destroys the Value object and releases associated resources.
  
  Cleans up any allocated memory and resets the internal state.
  Ensures proper resource management when a Json::Value object goes out of scope or is explicitly deleted.
  */
  ~Value();

  Value& operator=(const Value& other);
  Value& operator=(Value&& other) noexcept;

  /*!
  \brief Swaps the contents of this Value with another.
  
  Exchanges the entire contents of this Value object with those of another Value object.
  This includes the main payload, comments, and position information.
  Provides an efficient way to reorganize JSON data without deep copying.
  
  \param other The Value object to swap contents with.
  */
  void swap(Value& other);

  /*!
  \brief Swaps the internal payload with another Value object.
  
  Efficiently exchanges the internal data representation between this Value object and another.
  This operation is used extensively during JSON parsing to update the current value being processed without unnecessary copying or memory allocation.
  
  \param other The Value object with which to swap the internal payload.
  */
  void swapPayload(Value& other);

  /*!
  \brief Creates a deep copy of another Value object.
  
  Performs a deep copy of the provided Value object, including its payload and metadata.
  This function is used internally for copying Value objects.
  
  \param other The Value object to be copied.
  */
  void copy(const Value& other);

  /*!
  \brief Copies the payload from another Value object.
  
  Releases the current payload and duplicates the payload of the provided Value object.
  This function is used internally for deep copying of Value objects.
  
  \param other The Value object whose payload is to be copied.
  */
  void copyPayload(const Value& other);

  /*!
  \brief Returns the type of the held value.
  
  Determines the type of the JSON value stored in this Value object.
  This function is essential for type-specific operations and processing logic throughout the library.
  
  \return An enumeration of type ValueType representing the type of the held JSON value (e.g., null, integer, string, array, object).
  */
  ValueType type() const;

  bool operator<(const Value& other) const;
  bool operator<=(const Value& other) const;
  bool operator>=(const Value& other) const;
  bool operator>(const Value& other) const;
  bool operator==(const Value& other) const;
  bool operator!=(const Value& other) const;
  /*!
  \brief Compares this JSON value with another.
  
  Performs a three-way comparison between this JSON value and another.
  The comparison is based on the JSON value types and their contents.
  
  \param other The JSON value to compare against.
  
  \return An integer indicating the result of the comparison: -1 if this value is less than the other, 1 if greater, and 0 if equal.
  */
  int compare(const Value& other) const;

  /*!
  \brief Returns the value as a C-style string.
  
  Retrieves the stored string value as a const char* pointer.
  Ensures that the Value object contains a string type.
  If the internal string is null, returns nullptr.
  
  \return A pointer to the C-style string representation of the value, or nullptr if the internal string is null.
  */
  const char* asCString() const;
#if JSONCPP_USE_SECURE_MEMORY
  unsigned getCStringLength() const;

#endif
  /*!
  \brief Converts the JSON value to a string representation.
  
  Converts the JSON value to its string representation based on its type.
  Handles null, string, boolean, integer, unsigned integer, and real number types.
  For non-convertible types, it fails with an error message.
  
  \return A string representation of the JSON value. Empty string for null values, 'true' or 'false' for booleans, and appropriate string conversions for other supported types.
  */
  String asString() const;

  /*!
  \brief Retrieves the string value of a JSON element.
  
  Extracts the string value from the JSON element if it is of string type.
  Sets the begin and end pointers to the start and end of the string content, respectively.
  This method is useful for efficient string handling without copying.
  
  \param begin Pointer to be set to the start of the string content.
  \param end Pointer to be set to the end of the string content.
  
  \return True if the operation was successful, false if the value is not a string or is null.
  */
  bool getString(char const** begin, char const** end) const;
  /*!
  \brief Converts the value to an integer.
  
  Attempts to convert the current value to an integer.
  Supports conversion from integer, unsigned integer, real, null, and boolean types.
  Throws an exception if the conversion is not possible.
  
  \return The integer representation of the value. Returns 0 for null values and 0 or 1 for boolean values.
  */
  Int asInt() const;
  /*!
  \brief Converts the value to an unsigned integer.
  
  Converts the JSON value to an unsigned integer.
  Handles various input types including integers, unsigned integers, floating-point numbers, booleans, and null values.
  Throws an exception if the conversion is not possible.
  
  \return The unsigned integer representation of the value. Returns 0 for null values and 1 or 0 for boolean true or false respectively.
  */
  UInt asUInt() const;
#if defined(JSON_HAS_INT64)
  /*!
  \brief Converts the value to a 64-bit signed integer.
  
  Attempts to convert the current value to a 64-bit signed integer.
  Handles various internal types including integer, unsigned integer, real, null, and boolean.
  Throws an exception if the conversion is not possible.
  
  \return The value as a 64-bit signed integer. Returns 0 for null values and 1 or 0 for boolean values.
  */
  Int64 asInt64() const;
  /*!
  \brief Converts the value to an unsigned 64-bit integer.
  
  Attempts to convert the current value to an unsigned 64-bit integer.
  Handles various input types including integers, unsigned integers, floating-point numbers, null values, and booleans.
  Throws an exception if the conversion is not possible or the value is out of range.
  
  \return The value as an unsigned 64-bit integer. Returns 0 for null values and 0 or 1 for boolean values.
  */
  UInt64 asUInt64() const;
#endif
  /*!
  \brief Retrieves the value as the largest supported integer type.
  
  Converts the JSON value to the largest supported integer type.
  This function is used internally by writer implementations to ensure accurate representation of integer values in the output JSON, regardless of the underlying integer type.
  
  \return The value as the largest supported integer type (either int or int64_t, depending on the build configuration).
  */
  LargestInt asLargestInt() const;
  /*!
  \brief Retrieves the value as the largest supported unsigned integer type.
  
  Converts the stored value to the largest unsigned integer type available on the platform.
  This function provides a consistent interface for accessing unsigned integer values, regardless of the underlying platform-specific integer size.
  
  \return The value as the largest supported unsigned integer type (UInt or UInt64, depending on platform configuration).
  */
  LargestUInt asLargestUInt() const;
  /*!
  \brief Converts the JSON value to a float.
  
  Attempts to convert the stored JSON value to a float.
  Handles various data types including integers, unsigned integers, floating-point numbers, null values, and booleans.
  Throws an exception for non-convertible types.
  
  \return The float representation of the JSON value. Returns 0.0 for null values, and 1.0 or 0.0 for boolean true or false respectively.
  */
  float asFloat() const;
  /*!
  \brief Converts the value to a double-precision floating-point number.
  
  Converts the internal representation of the JSON value to a double.
  Handles various value types including integers, unsigned integers, real numbers, null, and boolean.
  Throws an exception for non-convertible types.
  
  \return The double-precision floating-point representation of the value. Returns 0.0 for null, and 1.0 or 0.0 for boolean true or false respectively.
  */
  double asDouble() const;
  /*!
  \brief Converts the value to a boolean.
  
  Interprets the value as a boolean.
  Numeric values are considered true if non-zero.
  Strings and other types are not convertible to bool and will cause a runtime error.
  
  \return The boolean representation of the value. Returns false for null values.
  */
  bool asBool() const;

  /*!
  \brief Checks if the value is null.
  
  Determines whether the current Value object represents a JSON null value.
  This function is useful for distinguishing between null and other value types in JSON data.
  
  \return true if the value is null, false otherwise.
  */
  bool isNull() const;
  /*!
  \brief Checks if the value is a boolean.
  
  Determines whether the current Value object represents a boolean value.
  This function is useful for type checking before performing boolean-specific operations.
  
  \return true if the value is a boolean, false otherwise.
  */
  bool isBool() const;
  /*!
  \brief Checks if the value is an integer within the platform-specific range.
  
  Determines if the current value can be represented as a 32-bit signed integer.
  This includes checking if the value is of integer type, or if it's a floating-point number that can be exactly represented as an integer within the valid range.
  
  \return true if the value is an integer within the platform's int range, false otherwise.
  */
  bool isInt() const;
  /*!
  \brief Checks if the value can be represented as a 64-bit signed integer.
  
  Determines whether the current value can be accurately represented as a 64-bit signed integer.
  This includes checking for integral values within the int64 range, as well as floating-point values that are effectively integral and within the int64 bounds.
  
  \return true if the value can be represented as a 64-bit signed integer, false otherwise.
  */
  bool isInt64() const;
  /*!
  \brief Checks if the value is an unsigned integer.
  
  Determines whether the current value can be represented as an unsigned integer.
  This includes checking for non-negative integers within the range of unsigned int, as well as floating-point numbers that can be exactly represented as unsigned integers.
  
  \return true if the value is an unsigned integer, false otherwise.
  */
  bool isUInt() const;
  /*!
  \brief Checks if the value is an unsigned 64-bit integer.
  
  Determines whether the current value can be represented as an unsigned 64-bit integer.
  This includes positive integers up to 2^64 - 1 and real numbers that can be exactly represented as unsigned 64-bit integers.
  
  \return true if the value is an unsigned 64-bit integer, false otherwise.
  */
  bool isUInt64() const;
  /*!
  \brief Checks if the value is an integral number.
  
  Determines whether the stored value is an integer or can be accurately represented as an integer.
  This includes values of type intValue and uintValue, as well as certain realValue types that are effectively integral.
  
  \return True if the value is an integral number, false otherwise.
  */
  bool isIntegral() const;
  /*!
  \brief Checks if the value is a double or can be represented as a double.
  
  Determines whether the value is a double-precision floating-point number or can be accurately represented as one.
  This includes integer types that can be converted to double without loss of precision.
  
  \return true if the value is a double or can be represented as a double, false otherwise.
  */
  bool isDouble() const;
  /*!
  \brief Checks if the value is numeric.
  
  Determines whether the current value represents a numeric type.
  This function is equivalent to checking if the value is a double.
  
  \return true if the value is numeric (double), false otherwise.
  */
  bool isNumeric() const;
  /*!
  \brief Checks if the value is a string.
  
  Determines whether the current Json::Value instance represents a string type.
  This method is useful for type checking before performing string-specific operations.
  
  \return true if the value is a string, false otherwise.
  */
  bool isString() const;
  /*!
  \brief Checks if the value is an array.
  
  Determines whether the current JSON value represents an array.
  This method is useful for type checking before performing array-specific operations or when validating JSON structure.
  
  \return true if the value is an array, false otherwise.
  */
  bool isArray() const;
  /*!
  \brief Checks if the value is a JSON object.
  
  Determines whether the current value represents a JSON object.
  This method is useful for type checking before performing object-specific operations, ensuring type safety in JSON manipulation.
  
  \return true if the value is a JSON object, false otherwise.
  */
  bool isObject() const;

  template <typename T> T as() const JSONCPP_TEMPLATE_DELETE;
  template <typename T> bool is() const JSONCPP_TEMPLATE_DELETE;

  /*!
  \brief Checks if the value can be converted to the specified type.
  
  Determines whether the current value can be converted to the given ValueType.
  The conversion rules depend on the target type and the current value's type and content.
  
  \param other The target ValueType to check for convertibility.
  
  \return True if the value can be converted to the specified type, false otherwise.
  */
  bool isConvertibleTo(ValueType other) const;

  /*!
  \brief Returns the number of elements in the JSON value.
  
  Determines the size of the JSON value.
  For arrays, returns the number of elements.
  For objects, returns the number of key-value pairs.
  For all other types (null, int, uint, real, bool, string), returns 0.
  
  \return The number of elements in the JSON value. For non-container types, returns 0.
  */
  ArrayIndex size() const;

  /*!
  \brief Checks if the JSON value is empty.
  
  Determines if the JSON value is empty.
  For null values, arrays, and objects, it checks if the size is zero.
  For other types, it always returns false.
  
  \return True if the value is an empty null, array, or object; false otherwise.
  */
  bool empty() const;

  explicit operator bool() const;

  /*!
  \brief Clears the contents of the Value.
  
  Removes all elements from an array or object Value.
  Has no effect on other Value types.
  Asserts if called on a null Value.
  */
  void clear();

  /*!
  \brief Resizes the array.
  
  Changes the size of the array.
  If the new size is larger, new elements are added with null values.
  If the new size is smaller, excess elements are removed.
  If the current value is null, it is converted to an empty array before resizing.
  
  \param newSize The new size of the array.
  */
  void resize(ArrayIndex newSize);

  Value& operator[](ArrayIndex index);
  Value& operator[](int index);

  const Value& operator[](ArrayIndex index) const;
  const Value& operator[](int index) const;

  /*!
  \brief Retrieves a value from an array at the specified index.
  
  Accesses an element in the JSON array at the given index.
  If the index is out of bounds or the value is null, returns the provided default value instead.
  
  \param index The zero-based index of the element to retrieve.
  \param defaultValue The value to return if the element at the specified index is not found or is null.
  
  \return The value at the specified index, or the default value if the index is invalid or the element is null.
  */
  Value get(ArrayIndex index, const Value& defaultValue) const;

  /*!
  \brief Checks if an index is valid for the current array.
  
  Determines whether the given index is within the bounds of the current JSON array.
  This function is used internally for safe array access and JSON path resolution.
  
  \param index The zero-based index to check for validity.
  
  \return true if the index is valid for the current array, false otherwise.
  */
  bool isValidIndex(ArrayIndex index) const;

  /*!
  \brief Appends a value to the array.
  
  Adds the given value to the end of the array.
  If this Value is not an array, it is first converted to an empty array before appending.
  
  \param value The Value to be appended to the array.
  
  \return A reference to the modified Value object.
  */
  Value& append(const Value& value);
  /*!
  \brief Appends a value to the array.
  
  Adds the given value to the end of the array.
  If the current value is null, it is first converted to an empty array.
  Asserts if the current value is not null or an array.
  
  \param value The value to be appended to the array.
  
  \return A reference to the appended value within the array.
  */
  Value& append(Value&& value);

  /*!
  \brief Inserts a value at a specified index in the array.
  
  Inserts a copy of the provided value at the specified index in the array.
  Shifts existing elements to make room for the new value.
  If the index is out of bounds, the behavior is undefined.
  
  \param index The position at which to insert the new value.
  \param newValue The value to be inserted into the array.
  
  \return True if the insertion was successful, false otherwise.
  */
  bool insert(ArrayIndex index, const Value& newValue);
  /*!
  \brief Inserts a value at a specified index in the array.
  
  Inserts the given value at the specified index in the array, shifting existing elements to make room.
  Requires the Value to be of arrayValue type.
  If the index is out of bounds, the operation fails.
  
  \param index The position at which to insert the new value. Must be less than or equal to the current array size.
  \param newValue The value to be inserted into the array.
  
  \return true if the insertion was successful, false if the index was out of bounds.
  */
  bool insert(ArrayIndex index, Value&& newValue);

  Value& operator[](const char* key);

  const Value& operator[](const char* key) const;

  Value& operator[](const String& key);

  const Value& operator[](const String& key) const;

  Value& operator[](const StaticString& key);

  /*!
  \brief Retrieves a value from the JSON object using a key.
  
  Searches for a value associated with the given key in the JSON object.
  If the key is not found, returns the provided default value.
  
  \param key The null-terminated string representing the key to search for in the JSON object.
  \param defaultValue The value to return if the key is not found in the JSON object.
  
  \return The value associated with the key if found, otherwise the default value.
  */
  Value get(const char* key, const Value& defaultValue) const;

  /*!
  \brief Retrieves a value from the JSON object using a key range.
  
  Searches for a value in the JSON object using the provided key range.
  If the key is found, returns the corresponding value; otherwise, returns the specified default value.
  
  \param begin Pointer to the beginning of the key string.
  \param end Pointer to the end of the key string.
  \param defaultValue Value to return if the key is not found.
  
  \return The value associated with the key if found, otherwise the default value.
  */
  Value get(const char* begin, const char* end,
            const Value& defaultValue) const;

  /*!
  \brief Retrieves a value from the JSON object using a string key.
  
  Searches for the specified key in the JSON object.
  If the key is found, returns the corresponding value.
  Otherwise, returns the provided default value.
  
  \param key The string key to search for in the JSON object.
  \param defaultValue The value to return if the key is not found in the JSON object.
  
  \return The value associated with the key if found, otherwise the default value.
  */
  Value get(const String& key, const Value& defaultValue) const;

  /*!
  \brief Searches for a key in the JSON object.
  
  Searches for a key within the current JSON object using the provided character range.
  If the current value is not an object or is null, the function returns nullptr.
  
  \param begin Pointer to the beginning of the key string.
  \param end Pointer to the end of the key string (exclusive).
  
  \return Pointer to the Value associated with the key if found, nullptr otherwise.
  */
  Value const* find(char const* begin, char const* end) const;

  /*!
  \brief Searches for a value associated with the given key.
  
  Performs a search operation to find a value in the JSON structure using the provided key.
  This const version allows for read-only access to the JSON data.
  
  \param key The string key to search for in the JSON structure.
  
  \return A pointer to the const Value if found, or null if the key does not exist.
  */
  Value const* find(const String& key) const;

  /*!
  \brief Retrieves or creates a nested Value object.
  
  Ensures the existence of a nested Value object within the current object.
  If the current Value is null, it is converted to an object.
  The function then resolves or creates the nested Value at the specified path.
  
  \param begin The start of the path string to the nested Value.
  \param end The end of the path string to the nested Value.
  
  \return A pointer to the resolved or created nested Value object.
  */
  Value* demand(char const* begin, char const* end);

  /*!
  \brief Removes a member from the JSON object.
  
  Removes the specified member from the JSON object if it exists.
  If the Value is not an object or is null, the function has no effect.
  
  \param key The name of the member to remove from the object.
  */
  void removeMember(const char* key);

  /*!
  \brief Removes a member from the JSON object.
  
  Removes the specified member from the JSON object if it exists.
  This function is a convenience wrapper that converts the string key to a C-style string before calling the main implementation.
  
  \param key The name of the member to be removed from the JSON object.
  */
  void removeMember(const String& key);

  /*!
  \brief Removes a member from the JSON object.
  
  Removes the specified member from the JSON object if it exists.
  Optionally stores the removed value in a provided pointer.
  
  \param key The name of the member to remove.
  \param removed Pointer to a Value object where the removed member's value will be stored. If null, the removed value is discarded.
  
  \return True if the member was found and removed, false otherwise.
  */
  bool removeMember(const char* key, Value* removed);

  /*!
  \brief Removes a member from the JSON object.
  
  Removes a member with the specified key from the JSON object.
  If the member is found and removed, it can optionally be stored in the provided Value pointer.
  
  \param key The key of the member to be removed.
  \param removed Optional pointer to a Value object where the removed member will be stored if found. If null, the removed member is discarded.
  
  \return True if a member with the specified key was found and removed, false otherwise.
  */
  bool removeMember(String const& key, Value* removed);

  /*!
  \brief Removes a member from an object.
  
  Removes a member from the JSON object if it exists.
  If the Value is not an object, the function does nothing.
  The removed member can optionally be returned.
  
  \param begin Pointer to the beginning of the member name string.
  \param end Pointer to the end of the member name string.
  \param removed Pointer to a Value object that will receive the removed member. If null, the removed member is discarded.
  
  \return True if a member was successfully removed, false otherwise.
  */
  bool removeMember(const char* begin, const char* end, Value* removed);

  /*!
  \brief Removes an element from the array at the specified index.
  
  Removes the element at the given index from the array and optionally returns the removed value.
  If the index is valid, the function shifts all subsequent elements to fill the gap.
  This operation is only valid for array-type Values.
  
  \param index The zero-based index of the element to remove.
  \param removed Pointer to a Value object that will receive the removed element. If null, the removed element is discarded.
  
  \return True if the element was successfully removed, false if the index was out of range or if the Value is not an array.
  */
  bool removeIndex(ArrayIndex index, Value* removed);

  /*!
  \brief Checks if a member with the specified key exists.
  
  Determines whether the current JSON object contains a member with the given key.
  This function is a const member, ensuring that it doesn't modify the object's state.
  
  \param key The null-terminated string representing the key to search for in the JSON object.
  
  \return True if a member with the specified key exists, false otherwise.
  */
  bool isMember(const char* key) const;

  /*!
  \brief Checks if a key exists in the JSON object.
  
  Determines whether the current JSON object contains a member with the specified key.
  This function is useful for verifying the presence of a key before attempting to access its value.
  
  \param key The key to search for in the JSON object.
  
  \return True if the key exists in the object, false otherwise.
  */
  bool isMember(const String& key) const;

  /*!
  \brief Checks if a member exists within a specified range.
  
  Determines whether a member with a name defined by the given character range exists in the JSON object.
  This method is useful for checking the presence of a member without constructing a temporary string.
  
  \param begin Pointer to the start of the member name.
  \param end Pointer to the end of the member name (exclusive).
  
  \return True if a member with the specified name exists, false otherwise.
  */
  bool isMember(const char* begin, const char* end) const;

  /*!
  \brief Retrieves the names of all members in a JSON object.
  
  Returns a vector of strings containing the names of all members (keys) in the JSON object.
  If the value is null, returns an empty vector.
  Throws an assertion if the value is neither null nor an object.
  
  \return A vector of strings containing the member names of the JSON object.
  */
  Members getMemberNames() const;

  JSONCPP_DEPRECATED("Use setComment(String const&) instead.")
  void setComment(const char* comment, CommentPlacement placement) {
    setComment(String(comment, strlen(comment)), placement);
  }

  /*!
  \brief Sets a comment for the JSON value.
  
  Associates a comment with the JSON value.
  Allows for different comment placements relative to the value.
  This function is typically used during JSON parsing to preserve comments from the original input.
  
  \param comment The content of the comment to be set.
  \param len The length of the comment string.
  \param placement The position of the comment relative to the JSON value (e.g., before or after).
  */
  void setComment(const char* comment, size_t len, CommentPlacement placement) {
    setComment(String(comment, len), placement);
  }

  /*!
  \brief Sets a comment for the JSON value.
  
  Assigns a comment to the JSON value at the specified placement.
  The comment must start with a forward slash ('/') and will have any trailing newline removed.
  
  \param comment The comment string to be set. Must start with a '/' character.
  \param placement The placement of the comment relative to the JSON value.
  */
  void setComment(String comment, CommentPlacement placement);
  /*!
  \brief Checks for the presence of a comment in a specific placement.
  
  Determines whether the JSON value has an associated comment in the specified placement.
  This function is used to identify comments before, after, or on the same line as the value, which is crucial for maintaining comment integrity during JSON serialization.
  
  \param placement The position of the comment to check for (e.g., before, after, or same line).
  
  \return True if a comment exists in the specified placement, false otherwise.
  */
  bool hasComment(CommentPlacement placement) const;

  /*!
  \brief Retrieves a comment associated with the JSON value.
  
  Fetches a comment of the specified placement type from the JSON value.
  This function is used to access comments that were preserved during parsing or added programmatically.
  
  \param placement The position of the comment relative to the JSON value. Can be one of the CommentPlacement enum values.
  
  \return The comment string associated with the specified placement. Returns an empty string if no comment exists for the given placement.
  */
  String getComment(CommentPlacement placement) const;

  /*!
  \brief Converts the JSON value to a formatted string representation.
  
  Generates a string representation of the JSON value with proper indentation and line breaks.
  Includes any comments associated with the value.
  Uses default StreamWriterBuilder settings for formatting.
  
  \return A formatted string representation of the JSON value.
  */
  String toStyledString() const;

  /*!
  \brief Returns an iterator to the beginning of the JSON value.
  
  Provides a const_iterator to the start of the JSON value.
  Works for array and object types, returning an iterator to their first element.
  For other types, returns an empty iterator.
  
  \return A const_iterator pointing to the beginning of the JSON value if it's an array or object, otherwise an empty iterator.
  */
  const_iterator begin() const;
  /*!
  \brief Returns an iterator to the end of the JSON value.
  
  Provides a const_iterator to the end of the JSON value for array and object types.
  For other types, returns an empty iterator.
  This function is crucial for range-based iteration and validation of JSON data structures.
  
  \return A const_iterator to the end of the JSON value for arrays and objects, or an empty iterator for other types.
  */
  const_iterator end() const;

  /*!
  \brief Returns an iterator to the beginning of the JSON value.
  
  Provides an iterator to the start of the JSON value if it's an array or object.
  For other types, returns an empty iterator.
  This function enables iteration over the elements of arrays and objects.
  
  \return An iterator pointing to the first element if the value is an array or object, otherwise an empty iterator.
  */
  iterator begin();
  /*!
  \brief Returns an iterator to the end of the JSON value.
  
  Provides an iterator pointing to the end of the JSON value.
  This function is only meaningful for array and object types.
  For other types, it returns an empty iterator.
  
  \return An iterator to the end of the JSON value for array and object types, or an empty iterator for other types.
  */
  iterator end();

  /*!
  \brief Returns a reference to the first element in the array.
  
  Provides access to the first element of the JSON array.
  This function assumes that the Value object represents an array and that it is not empty.
  It is equivalent to dereferencing the iterator returned by begin().
  
  \return A constant reference to the first Value in the array.
  */
  const Value& front() const;

  /*!
  \brief Returns a reference to the first element in the array.
  
  Provides access to the first element of the JSON array.
  This function assumes that the Value object represents an array and that the array is not empty.
  It is equivalent to dereferencing the begin iterator.
  
  \return A reference to the first Value in the array.
  */
  Value& front();

  /*!
  \brief Returns a reference to the last element in the array.
  
  Provides access to the last element of an array-type Value.
  This function assumes the Value is an array and contains at least one element.
  It is the caller's responsibility to ensure these conditions are met.
  
  \return A const reference to the last element in the array.
  */
  const Value& back() const;

  /*!
  \brief Returns a reference to the last element in the array.
  
  Provides access to the last element of an array-type Value.
  This function assumes the Value is an array and contains at least one element.
  It is the caller's responsibility to ensure these conditions are met.
  
  \return A reference to the last element in the array.
  */
  Value& back();

  /*!
  \brief Sets the starting offset of the JSON value.
  
  Defines the starting position of this JSON value within the original parsed input.
  Used in conjunction with setOffsetLimit to maintain positional information for error reporting, formatting, and structure preservation.
  
  \param start The starting offset position of the JSON value in the input string.
  */
  void setOffsetStart(ptrdiff_t start);
  /*!
  \brief Sets the end position of the JSON value in the input stream.
  
  Defines the ending offset of the current JSON value within the parsed document.
  Used in conjunction with setOffsetStart to track the exact position and boundaries of each JSON element.
  
  \param limit The ending offset (exclusive) of the JSON value in the input stream.
  */
  void setOffsetLimit(ptrdiff_t limit);
  /*!
  \brief Retrieves the starting offset of the JSON value.
  
  Returns the position where this JSON value begins in the input stream.
  This is useful for error reporting and locating specific elements within the JSON structure.
  
  \return The starting offset of the JSON value as a ptrdiff_t.
  */
  ptrdiff_t getOffsetStart() const;
  /*!
  \brief Retrieves the offset limit of the JSON value.
  
  Returns the end position of the JSON value in the input stream.
  This function is used in conjunction with getOffsetStart to define the span of a JSON value, enabling precise error reporting and token creation during JSON parsing.
  
  \return The offset limit (end position) of the JSON value in the input stream.
  */
  ptrdiff_t getOffsetLimit() const;

private:
  /*!
  \brief Sets the type of the JSON value.
  
  Updates the internal representation of the JSON value to the specified type.
  This is a low-level function that directly modifies the value's type without performing any data conversion or validation.
  
  \param v The new ValueType to set for this JSON value.
  */
  void setType(ValueType v) {
    bits_.value_type_ = static_cast<unsigned char>(v);
  }
  /*!
  \brief Checks if the value's payload is dynamically allocated.
  
  Determines whether the internal string payload of the Value object has been dynamically allocated.
  This is crucial for memory management operations, particularly when copying or modifying string values.
  
  \return True if the payload is dynamically allocated, false otherwise.
  */
  bool isAllocated() const { return bits_.allocated_; }
  /*!
  \brief Sets the allocation status of the Value.
  
  Modifies the internal flag indicating whether the Value's memory is allocated.
  This is a private method used for internal memory management.
  
  \param v Boolean value indicating the allocation status to be set.
  */
  void setIsAllocated(bool v) { bits_.allocated_ = v; }

  /*!
  \brief Initializes the basic properties of a Value object.
  
  Sets the type, allocation status, and resets comments and internal counters of the Value object.
  This private method is used internally for basic initialization of Value instances.
  
  \param type The ValueType to set for this Value object.
  \param allocated A boolean indicating whether this Value object is allocated (true) or not (false).
  */
  void initBasic(ValueType type, bool allocated = false);
  /*!
  \brief Duplicates the payload of another Value object.
  
  Copies the content of another Value object to this one, including its type and data.
  For complex types like strings, arrays, and objects, performs a deep copy.
  
  \param other The Value object whose payload is to be duplicated.
  */
  void dupPayload(const Value& other);
  /*!
  \brief Releases the allocated memory associated with the Value.
  
  Frees memory resources based on the current type of the Value.
  For string types, it releases the allocated string memory if necessary.
  For array and object types, it deletes the associated map.
  This function is used internally for memory management.
  */
  void releasePayload();
  /*!
  \brief Duplicates metadata from another Value object.
  
  Copies the comments and position information from the given Value object to this one.
  Used internally for maintaining metadata during Value operations.
  
  \param other The Value object from which to copy metadata.
  */
  void dupMeta(const Value& other);

  /*!
  \brief Resolves or creates a reference to a JSON object member.
  
  Ensures the Value is an object and returns a reference to the member associated with the given key.
  If the key doesn't exist, it creates a new null member.
  
  \param key The string key of the object member to resolve or create.
  
  \return A reference to the Value associated with the key, either existing or newly created.
  */
  Value& resolveReference(const char* key);
  /*!
  \brief Resolves or creates a reference to a JSON object member.
  
  Ensures the Value is an object and returns a reference to the member specified by the key.
  If the member doesn't exist, it creates a null member with the given key and returns a reference to it.
  
  \param key The start of the string containing the member's key.
  \param end The end of the string containing the member's key.
  
  \return A reference to the resolved or created Value object member.
  */
  Value& resolveReference(const char* key, const char* end);

  union ValueHolder {
    LargestInt int_;
    LargestUInt uint_;
    double real_;
    bool bool_;
    char* string_;
    ObjectValues* map_;
  } value_;

  struct {

    unsigned int value_type_ : 8;

    unsigned int allocated_ : 1;
  } bits_;

  /*!
  \class Comments
  \brief Manages comments associated with JSON values.
  
  Provides functionality to store, retrieve, and check for comments in different placement slots.
  Uses a unique pointer to an array of strings for efficient storage and offers methods to set, get, and check the presence of comments.
  Supports copy and move operations for flexible usage within JSON structures.
  */
  class Comments {
  public:
    Comments() = default;
    /*!
    \brief Creates a copy of another Comments object.
    
    Constructs a new Comments object by performing a deep copy of the provided Comments object.
    This copy constructor ensures that the newly created object has its own independent copy of the comment data.
    
    \param that The source Comments object to be copied.
    */
    Comments(const Comments& that);
    /*!
    \brief Constructs a Comments object by moving from another instance.
    
    Performs a move construction, transferring ownership of the comment data from the source object to the newly created object.
    This operation leaves the source object in a valid but unspecified state.
    
    \param that The source Comments object to move from. Its resources will be transferred to this object.
    */
    Comments(Comments&& that) noexcept;
    Comments& operator=(const Comments& that);
    Comments& operator=(Comments&& that) noexcept;
    /*!
    \brief Checks for the presence of a comment in a specific placement.
    
    Determines whether a comment exists for the given comment placement slot.
    This function is used internally to verify if a comment has been set for a particular position within the JSON value.
    
    \param slot The comment placement slot to check.
    
    \return true if a non-empty comment exists in the specified slot, false otherwise.
    */
    bool has(CommentPlacement slot) const;
    /*!
    \brief Retrieves a comment for a specified placement slot.
    
    Accesses and returns the comment associated with the given CommentPlacement slot.
    If no comment exists for the specified slot, an empty string is returned.
    
    \param slot The CommentPlacement value indicating the desired comment's location.
    
    \return A String containing the comment text, or an empty string if no comment exists for the specified slot.
    */
    String get(CommentPlacement slot) const;
    /*!
    \brief Sets a comment for a specific placement slot.
    
    Assigns a comment to the specified comment placement slot.
    If the slot is valid and the internal storage hasn't been initialized, it creates the storage before setting the comment.
    
    \param slot The placement slot for the comment. Must be a valid CommentPlacement value.
    \param comment The comment string to be set for the specified slot.
    */
    void set(CommentPlacement slot, String comment);

  private:
    using Array = std::array<String, numberOfCommentPlacement>;
    std::unique_ptr<Array> ptr_;
  };
  Comments comments_;

  ptrdiff_t start_;
  ptrdiff_t limit_;
};

/*!
\brief Converts the JSON value to a boolean.

Provides a type-safe way to retrieve the stored value as a boolean.
This is a template specialization for bool type, which internally calls the asBool() method.

\return The boolean representation of the JSON value.
*/
template <> inline bool Value::as<bool>() const { return asBool(); }
template <> inline bool Value::is<bool>() const { return isBool(); }

/*!
\brief Converts the JSON value to an integer.

Provides a type-safe way to retrieve the stored value as an integer.
This is a template specialization for Int type, which internally calls the asInt() method.

\return The integer representation of the stored JSON value.
*/
template <> inline Int Value::as<Int>() const { return asInt(); }
template <> inline bool Value::is<Int>() const { return isInt(); }

/*!
\brief Converts the JSON value to an unsigned integer.

Provides a templated conversion of the JSON value to an unsigned integer type.
This is a specialized template implementation for UInt type, which internally calls the asUInt() method.

\return The JSON value converted to an unsigned integer.
*/
template <> inline UInt Value::as<UInt>() const { return asUInt(); }
template <> inline bool Value::is<UInt>() const { return isUInt(); }

#if defined(JSON_HAS_INT64)
/*!
\brief Converts the JSON value to a 64-bit integer.

Provides a type-safe conversion of the JSON value to an Int64.
This is a template specialization for Int64 type, which internally calls the asInt64() method.

\return The JSON value as a 64-bit integer.
*/
template <> inline Int64 Value::as<Int64>() const { return asInt64(); }
template <> inline bool Value::is<Int64>() const { return isInt64(); }

/*!
\brief Converts the JSON value to an unsigned 64-bit integer.

Provides a type-safe way to convert the JSON value to an unsigned 64-bit integer.
This is a specialization of the template function for UInt64 type.

\return The JSON value as an unsigned 64-bit integer.
*/
template <> inline UInt64 Value::as<UInt64>() const { return asUInt64(); }
template <> inline bool Value::is<UInt64>() const { return isUInt64(); }
#endif

/*!
\brief Converts the JSON value to a double.

Provides a type-safe way to retrieve the stored value as a double.
This is a specialization of the generic 'as' template function for the double type.

\return The stored value converted to a double.
*/
template <> inline double Value::as<double>() const { return asDouble(); }
template <> inline bool Value::is<double>() const { return isDouble(); }

/*!
\brief Converts the JSON value to a string.

Provides a type-safe way to convert the JSON value to a string representation.
This is a specialization of the template function for String type.

\return The string representation of the JSON value.
*/
template <> inline String Value::as<String>() const { return asString(); }
/*!
\brief Checks if the value is a string.

Determines whether the current Json::Value object holds a string value.
This is a type-specific template specialization of the general 'is' method for string types.

\return true if the value is a string, false otherwise.
*/
template <> inline bool Value::is<String>() const { return isString(); }

/*!
\brief Converts the JSON value to a float.

Provides a type-safe way to retrieve the stored value as a float.
This is a specialization of the template function for float type.

\return The stored value converted to a float.
*/
template <> inline float Value::as<float>() const { return asFloat(); }
/*!
\brief Retrieves the value as a C-style string.

Provides a type-safe way to access the underlying JSON value as a const char* pointer.
This is a specialized template function for const char* type.

\return A pointer to the null-terminated string representation of the value.
*/
template <> inline const char* Value::as<const char*>() const {
  return asCString();
}

/*!
\class PathArgument
\brief Represents a component of a JSON path for navigation.

Encapsulates either an array index or an object key used in JSON traversal.
Provides different constructors for creating index-based or key-based path components.
Works in conjunction with the Path class to enable efficient navigation and manipulation of JSON structures.
*/
class JSON_API PathArgument {
public:
  friend class Path;

  PathArgument();
  /*!
  \brief Constructs a PathArgument representing an array index.
  
  Creates a PathArgument object that represents a specific index in a JSON array.
  This constructor is used when navigating through array elements in a JSON structure.
  
  \param index The zero-based index of the array element to access.
  */
  PathArgument(ArrayIndex index);
  /*!
  \brief Constructs a PathArgument representing a key in a JSON object.
  
  Initializes a PathArgument with a string key, setting its kind to 'kindKey'.
  This constructor is used when the path component represents a key in a JSON object, allowing for navigation through object structures.
  
  \param key Null-terminated string representing the key in a JSON object.
  */
  PathArgument(const char* key);
  /*!
  \brief Constructs a PathArgument representing a key in a JSON object.
  
  Creates a PathArgument instance for navigating to a specific key within a JSON object.
  The provided key is stored internally and the argument is marked as a key type.
  
  \param key The string key used to access a value in a JSON object.
  */
  PathArgument(String key);

private:
  enum Kind { kindNone = 0, kindIndex, kindKey };
  String key_;
  ArrayIndex index_{};
  Kind kind_{kindNone};
};

/*!
\class Path
\brief Facilitates navigation and manipulation of JSON data structures.

Provides functionality for creating, resolving, and modifying paths within JSON objects or arrays.
Supports both string-based path representations and PathArgument objects for traversing JSON structures, retrieving values, and creating or updating nested elements.
Enables efficient access to JSON data using index-based and key-based methods.
*/
class JSON_API Path {
public:
  /*!
  \brief Constructs a Path object from a string and optional PathArguments.
  
  Creates a Path object using a string representation of the path and up to five optional PathArgument objects.
  The constructor combines the string path with the provided PathArguments to form a complete path for JSON navigation.
  
  \param path String representation of the base path.
  \param a1 First optional PathArgument to be appended to the base path.
  \param a2 Second optional PathArgument to be appended to the base path.
  \param a3 Third optional PathArgument to be appended to the base path.
  \param a4 Fourth optional PathArgument to be appended to the base path.
  \param a5 Fifth optional PathArgument to be appended to the base path.
  */
  Path(const String& path, const PathArgument& a1 = PathArgument(),
       const PathArgument& a2 = PathArgument(),
       const PathArgument& a3 = PathArgument(),
       const PathArgument& a4 = PathArgument(),
       const PathArgument& a5 = PathArgument());

  /*!
  \brief Resolves a JSON path within a given root value.
  
  Traverses the JSON structure specified by the path, starting from the given root value.
  Handles both array indices and object keys.
  Returns the value at the specified path or a null value if the path is invalid.
  
  \param root The JSON value to use as the starting point for resolving the path.
  
  \return The value at the specified path, or a null value if the path is invalid or not found.
  */
  const Value& resolve(const Value& root) const;
  /*!
  \brief Resolves a JSON path within a given root value.
  
  Traverses the JSON structure following the path specified by the PathArgument objects.
  Returns the value at the end of the path if it exists, otherwise returns the default value.
  
  \param root The root JSON value to start the path resolution from.
  \param defaultValue The value to return if the path cannot be fully resolved.
  
  \return The JSON value at the end of the resolved path, or the default value if the path is invalid.
  */
  Value resolve(const Value& root, const Value& defaultValue) const;

  /*!
  \brief Creates or accesses nested JSON elements based on the path.
  
  Traverses the JSON structure using the stored path arguments.
  Creates new objects or arrays as needed to fulfill the path.
  If the path already exists, it simply navigates to the specified location.
  
  \param root The root JSON value to start the path traversal from.
  
  \return A reference to the JSON value at the end of the path, which may be newly created or pre-existing.
  */
  Value& make(Value& root) const;

private:
  using InArgs = std::vector<const PathArgument*>;
  using Args = std::vector<PathArgument>;

  /*!
  \brief Constructs a JSON path from a string representation and optional arguments.
  
  Parses the input string to create a structured JSON path.
  Recognizes special characters for array indices and object keys, and incorporates additional path arguments.
  Supports both static paths and dynamic paths with placeholders.
  
  \param path The string representation of the JSON path to be parsed.
  \param in A collection of additional path arguments to be incorporated into the path structure.
  */
  void makePath(const String& path, const InArgs& in);
  /*!
  \brief Adds a dynamic path component to the current path.
  
  Processes a dynamic path component during path construction.
  Validates the input argument against the expected kind (index or key) and appends it to the path's argument list if valid.
  
  \param path The current path string being constructed (unused in this function).
  \param in The collection of input arguments for dynamic path components.
  \param itInArg Iterator pointing to the current input argument being processed.
  \param kind The expected kind of the current path component (index or key).
  */
  void addPathInArg(const String& path, const InArgs& in,
                    InArgs::const_iterator& itInArg, PathArgument::Kind kind);
  /*!
  \brief Handles an invalid JSON path.
  
  Serves as a placeholder for error handling when an invalid JSON path is encountered during parsing.
  Currently implemented as a no-op, but may be extended for custom error reporting or exception throwing.
  
  \param path The invalid JSON path string.
  \param location The position in the path string where the error was detected.
  */
  static void invalidPath(const String& path, int location);

  Args args_;
};

/*!
\class ValueIteratorBase
\brief Provides a foundation for iterating over JSON objects with bidirectional capabilities.

Serves as a base class for iterators in the JsonCpp library, offering methods to traverse and manipulate JSON data structures.
Encapsulates functionality for accessing keys, values, and indices of JSON elements, along with comparison and arithmetic operations.
Supports both forward and backward iteration, allowing efficient navigation through JSON objects and arrays.
*/
class JSON_API ValueIteratorBase {
public:
  using iterator_category = std::bidirectional_iterator_tag;
  using size_t = unsigned int;
  using difference_type = int;
  using SelfType = ValueIteratorBase;

  bool operator==(const SelfType& other) const { return isEqual(other); }

  bool operator!=(const SelfType& other) const { return !isEqual(other); }

  difference_type operator-(const SelfType& other) const {
    return other.computeDistance(*this);
  }

  /*!
  \brief Retrieves the key of the current JSON element.
  
  Returns the key of the current element in the JSON object.
  For string keys, it creates a new Value object containing the key.
  For numeric keys, it returns a Value object with the index.
  
  \return A Value object representing the key of the current JSON element, either as a string or an index.
  */
  Value key() const;

  /*!
  \brief Retrieves the index of the current iterator position.
  
  Returns the index of the current element in the JSON object.
  If the element has a string key, it returns -1 (as UInt).
  For elements with numeric keys, it returns the actual index.
  
  \return The index of the current element if it has a numeric key, or -1 (as UInt) if it has a string key.
  */
  UInt index() const;

  /*!
  \brief Retrieves the name of the current member.
  
  Obtains the name of the current member in the JSON object being iterated.
  Returns an empty string if the iterator is not pointing to a valid member.
  
  \return A String containing the name of the current member, or an empty string if no valid member is found.
  */
  String name() const;

  JSONCPP_DEPRECATED("Use `key = name();` instead.")
  char const* memberName() const;

  /*!
  \brief Retrieves the name of the current member.
  
  Returns a pointer to the name of the current member in the JSON object iteration.
  Provides direct access to the member's key as a C-style string.
  
  \param end 
  
  \return A pointer to the member's name as a null-terminated string. Returns an empty string if the name is null.
  */
  char const* memberName(char const** end) const;

protected:
  /*!
  \brief Dereferences the iterator.
  
  Retrieves the value of the current element pointed to by the iterator.
  This function is used internally to access the value part of the key-value pair in a JSON object.
  
  \return A constant reference to the Value object at the current iterator position.
  */
  const Value& deref() const;
  /*!
  \brief Dereferences the iterator.
  
  Accesses the value pointed to by the iterator.
  Returns a reference to the second element of the key-value pair in the JSON object.
  
  \return A reference to the Value object at the current iterator position.
  */
  Value& deref();

  /*!
  \brief Advances the iterator to the next element.
  
  Increments the internal iterator to point to the next element in the JSON structure.
  This operation moves the iterator forward by one position.
  */
  void increment();

  /*!
  \brief Decrements the iterator.
  
  Moves the iterator one position backwards in the JSON structure.
  This operation is used for reverse iteration through JSON objects or arrays.
  */
  void decrement();

  /*!
  \brief Calculates the distance between two iterators.
  
  Computes the number of elements between this iterator and another.
  Returns zero if both iterators are null.
  
  \param other The iterator to calculate the distance to.
  
  \return The number of elements between this iterator and the other.
  */
  difference_type computeDistance(const SelfType& other) const;

  /*!
  \brief Compares this iterator with another for equality.
  
  Determines if this iterator points to the same position as another iterator.
  Handles null iterators as a special case.
  
  \param other The iterator to compare with this one.
  
  \return True if the iterators are equal, false otherwise.
  */
  bool isEqual(const SelfType& other) const;

  /*!
  \brief Copies the state of another iterator.
  
  Replicates the internal state of the given iterator.
  Assigns the current position and null status from the other iterator to this one.
  
  \param other The iterator whose state is to be copied.
  */
  void copy(const SelfType& other);

private:
  Value::ObjectValues::iterator current_;

  bool isNull_{true};

public:
  /*!
  \brief Constructs a default ValueIteratorBase.
  
  Initializes a ValueIteratorBase with a null internal iterator.
  Creates an iterator in an uninitialized state, typically used as a placeholder or for later assignment.
  */
  ValueIteratorBase();
  /*!
  \brief Constructs a ValueIteratorBase from an object iterator.
  
  Initializes a ValueIteratorBase with the provided object iterator.
  Sets the internal iterator to the given position and marks the iterator as non-null.
  This constructor is typically used internally to create iterators for JSON objects.
  
  \param current Object iterator to initialize this ValueIteratorBase with.
  */
  explicit ValueIteratorBase(const Value::ObjectValues::iterator& current);
};

/*!
\class ValueConstIterator
\brief Provides a constant iterator for traversing JSON Value objects.

Offers read-only access to elements within JSON data structures.
Inherits from ValueIteratorBase and supports standard iterator operations such as increment, decrement, and dereferencing.
Enables efficient traversal of JSON objects and arrays without allowing modifications to the underlying values, ensuring data integrity during iteration.
*/
class JSON_API ValueConstIterator : public ValueIteratorBase {
  friend class Value;

public:
  using value_type = const Value;

  using reference = const Value&;
  using pointer = const Value*;
  using SelfType = ValueConstIterator;

  ValueConstIterator();
  /*!
  \brief Constructs a constant iterator from a non-constant iterator.
  
  Creates a new ValueConstIterator by copying the state of a non-constant ValueIterator.
  This allows for converting a mutable iterator to a constant one, enabling read-only access to the JSON elements.
  
  \param other The non-constant iterator to be converted to a constant iterator.
  */
  ValueConstIterator(ValueIterator const& other);

private:
  /*!
  \brief Constructs a constant iterator for a JSON Value object.
  
  Initializes a ValueConstIterator with the provided object iterator.
  This constructor creates a read-only iterator for traversing JSON Value objects, allowing access to elements without modification.
  
  \param current Iterator to the current position in the JSON Value object.
  */
  explicit ValueConstIterator(const Value::ObjectValues::iterator& current);

public:
  SelfType& operator=(const ValueIteratorBase& other);

  SelfType operator++(int) {
    SelfType temp(*this);
    ++*this;
    return temp;
  }

  SelfType operator--(int) {
    SelfType temp(*this);
    --*this;
    return temp;
  }

  SelfType& operator--() {
    decrement();
    return *this;
  }

  SelfType& operator++() {
    increment();
    return *this;
  }

  reference operator*() const { return deref(); }

  pointer operator->() const { return &deref(); }
};

/*!
\class ValueIterator
\brief Provides a mutable iterator for traversing and modifying JSON values.

Offers standard iterator operations for navigating and altering JSON data structures.
Derived from ValueIteratorBase, it extends functionality to allow modification of underlying JSON elements.
Supports various initialization methods and includes overloaded operators for convenient iteration and value access, making it suitable for tasks that require reading and writing JSON data.
*/
class JSON_API ValueIterator : public ValueIteratorBase {
  friend class Value;

public:
  using value_type = Value;
  using size_t = unsigned int;
  using difference_type = int;
  using reference = Value&;
  using pointer = Value*;
  using SelfType = ValueIterator;

  ValueIterator();
  /*!
  \brief Constructs a ValueIterator from a ValueConstIterator.
  
  Creates a ValueIterator based on a ValueConstIterator.
  This constructor is designed to throw a runtime error, as converting from a const iterator to a non-const iterator is not allowed in this implementation.
  
  \param other The ValueConstIterator to convert from. Attempting to use this parameter will result in a runtime error.
  */
  explicit ValueIterator(const ValueConstIterator& other);
  ValueIterator(const ValueIterator& other);

private:
  /*!
  \brief Constructs a ValueIterator from an ObjectValues iterator.
  
  Initializes a ValueIterator using an iterator from the ObjectValues container.
  This constructor allows the creation of a ValueIterator that points to a specific position within a JSON object's key-value pairs.
  
  \param current An iterator to the current position in the ObjectValues container.
  */
  explicit ValueIterator(const Value::ObjectValues::iterator& current);

public:
  SelfType& operator=(const SelfType& other);

  SelfType operator++(int) {
    SelfType temp(*this);
    ++*this;
    return temp;
  }

  SelfType operator--(int) {
    SelfType temp(*this);
    --*this;
    return temp;
  }

  SelfType& operator--() {
    decrement();
    return *this;
  }

  SelfType& operator++() {
    increment();
    return *this;
  }

  reference operator*() const { return const_cast<reference>(deref()); }
  pointer operator->() const { return const_cast<pointer>(&deref()); }
};

/*!
\brief Swaps the contents of two Json::Value objects.

Exchanges the contents of two Json::Value objects efficiently.
Facilitates move semantics and resource management in Json::Value operations, particularly in move constructors and assignment operators.

\param a The first Json::Value object to be swapped.
\param b The second Json::Value object to be swapped.
*/
inline void swap(Value& a, Value& b) { a.swap(b); }

/*!
Returns a constant reference to the first element of the JSON array by dereferencing the iterator returned by begin().
Assumes the Value object represents a non-empty array.
*/
inline const Value& Value::front() const { return *begin(); }

/*!
Returns a reference to the first element of the array by dereferencing the begin iterator.
Assumes the Value object represents a non-empty array.
*/
inline Value& Value::front() { return *begin(); }

/*!
Returns a const reference to the last element of the array by decrementing the end iterator.
Assumes the Value is a non-empty array.
*/
inline const Value& Value::back() const { return *(--end()); }

/*!
Returns a reference to the last element of the array by decrementing the end iterator.
Assumes the Value is a non-empty array.
*/
inline Value& Value::back() { return *(--end()); }

} // namespace Json

#pragma pack(pop)

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif

#endif
