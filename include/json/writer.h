#ifndef JSON_WRITER_H_INCLUDED
#define JSON_WRITER_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "value.h"
#endif
#include <ostream>
#include <string>
#include <vector>

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING) && defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#pragma pack(push)
#pragma pack()

namespace Json {

class Value;

/*!
\class StreamWriter
\brief Provides an abstract interface for writing JSON data to output streams.

Defines a common framework for JSON stream writing implementations.
Offers a pure virtual write method for converting JSON Value objects to string representations and outputting them to streams.
Includes a nested Factory class for creating specific StreamWriter instances, enabling flexible and extensible JSON output functionality.
*/
class JSON_API StreamWriter {
protected:
  OStream* sout_;

public:
  /*!
  \brief Initializes a StreamWriter instance.
  
  Constructs a StreamWriter object with a null output stream pointer.
  This constructor sets up the base StreamWriter object, which can be further configured with a specific output stream for JSON writing operations.
  */
  StreamWriter();
  virtual ~StreamWriter();

  /*!
  \brief Writes JSON data to an output stream.
  
  Converts a JSON Value object to its string representation and writes it to the specified output stream.
  This is a pure virtual function that must be implemented by derived classes to define specific formatting and writing behavior.
  
  \param root The JSON Value object to be written to the stream.
  \param sout Pointer to the output stream where the JSON data will be written.
  
  \return An integer status code indicating the result of the write operation.
  */
  virtual int write(Value const& root, OStream* sout) = 0;

  /*!
  \class Factory
  \brief Defines an abstract factory for creating StreamWriter objects.
  
  Provides an interface for creating StreamWriter instances without specifying their concrete classes.
  Implements the factory pattern to allow flexible creation of different StreamWriter types, decoupling the instantiation process from the client code.
  Derived classes must implement the newStreamWriter() method to create specific StreamWriter implementations.
  */
  class JSON_API Factory {
  public:
    virtual ~Factory();

    /*!
    \brief Creates a new StreamWriter instance.
    
    Instantiates a new StreamWriter object for JSON serialization.
    This pure virtual function must be implemented by derived classes to provide specific StreamWriter implementations, allowing for flexible and customizable JSON writing processes.
    
    \return A pointer to a newly created StreamWriter object. The caller is responsible for managing the memory of the returned object.
    */
    virtual StreamWriter* newStreamWriter() const = 0;
  };
};

/*!
\brief Converts a JSON value to a formatted string.

Serializes a JSON value to a string using a custom stream writer.
Provides flexibility in formatting options while encapsulating the complexities of JSON serialization.

\param factory The factory used to create a custom stream writer for JSON serialization.
\param root The JSON value to be serialized.

\return A string representation of the JSON value.
*/
String JSON_API writeString(StreamWriter::Factory const& factory,
                            Value const& root);

/*!
\class StreamWriterBuilder
\brief Serves as a factory for creating StreamWriter objects with customizable JSON output formatting.

Manages settings for JSON output generation, including indentation, comment style, precision, and UTF-8 encoding.
Provides methods to create new StreamWriter instances, validate settings, and set default values.
Allows for flexible customization of JSON output formatting through a simple interface, enabling users to tailor the output to specific requirements.
*/
class JSON_API StreamWriterBuilder : public StreamWriter::Factory {
public:
  Json::Value settings_;

  /*!
  \brief Constructs a StreamWriterBuilder with default settings.
  
  Initializes a new StreamWriterBuilder instance and sets up default configuration for JSON output formatting.
  This constructor prepares the builder for creating StreamWriter objects with predefined settings, which can be customized later if needed.
  */
  StreamWriterBuilder();
  ~StreamWriterBuilder() override;

  /*!
  \brief Creates a new StreamWriter instance.
  
  Constructs a new StreamWriter based on the current settings.
  Configures the writer with indentation, comment style, precision, and other formatting options as specified in the builder's settings.
  
  \return A pointer to a newly created StreamWriter object. The caller is responsible for managing the memory of this object.
  */
  StreamWriter* newStreamWriter() const override;

  /*!
  \brief Validates the current settings of the StreamWriterBuilder.
  
  Checks if all keys in the settings_ member are valid for JSON stream writing.
  Valid keys include indentation, commentStyle, enableYAMLCompatibility, dropNullPlaceholders, useSpecialFloats, emitUTF8, precision, and precisionType.
  
  \param invalid A pointer to a Json::Value object that will store any invalid settings found. If null, the function returns false on the first invalid setting encountered.
  
  \return True if all settings are valid, or if invalid is non-null and no invalid settings were found. False otherwise.
  */
  bool validate(Json::Value* invalid) const;

  Value& operator[](const String& key);

  /*!
  \brief Initializes default settings for JSON stream writing.
  
  Populates a Json::Value object with predefined key-value pairs for JSON output configuration.
  Sets default values for comment style, indentation, YAML compatibility, null placeholders, special floats, UTF-8 encoding, and numerical precision.
  
  \param settings Pointer to a Json::Value object to be populated with default settings.
  */
  static void setDefaults(Json::Value* settings);
};

/*!
\class Writer
\brief Defines an interface for writing JSON data.

Serves as an abstract base class for implementing JSON writing strategies.
Provides a foundation for converting JSON Value objects into string representations, allowing derived classes to implement specific formatting and output methods.
*/
class JSON_API Writer {
public:
  virtual ~Writer();

  /*!
  \brief Converts a JSON Value to its string representation.
  
  Generates a string representation of the input JSON Value.
  The specific formatting of the output string depends on the derived class implementation.
  
  \param root The JSON Value object to be converted to a string.
  
  \return A string representation of the input JSON Value.
  */
  virtual String write(const Value& root) = 0;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
/*!
\class FastWriter
\brief Provides efficient JSON serialization with customizable output formatting.

Implements a fast and straightforward approach to converting JSON values into their string representation.
Offers options to customize the output format, including YAML compatibility, dropping null placeholders, and omitting ending line feeds.
Utilizes a simple string-based method for building JSON documents, prioritizing speed and simplicity in JSON serialization tasks.
*/
class JSON_API FastWriter : public Writer {
public:
  FastWriter();
  ~FastWriter() override = default;

  /*!
  \brief Enables YAML compatibility mode.
  
  Configures the writer to produce JSON output that is also valid YAML.
  This affects the formatting of the output to ensure compatibility with YAML parsers.
  */
  void enableYAMLCompatibility();

  /*!
  \brief Enables dropping of null placeholders in JSON output.
  
  Configures the FastWriter to omit null value placeholders when serializing JSON.
  This can result in more compact output by excluding keys with null values from the generated JSON string.
  */
  void dropNullPlaceholders();

  /*!
  \brief Configures the writer to omit the ending line feed.
  
  Sets the internal flag to omit the final line feed character when writing JSON output.
  This can be useful when the output needs to be embedded in other content without an extra newline at the end.
  */
  void omitEndingLineFeed();

public:
  /*!
  \brief Converts a JSON value to its string representation.
  
  Generates a JSON-formatted string from the provided JSON value.
  Applies the configured formatting options, such as YAML compatibility and omitting ending line feeds.
  
  \param root The JSON value to be converted to a string.
  
  \return A string containing the JSON representation of the input value.
  */
  String write(const Value& root) override;

private:
  /*!
  \brief Serializes a JSON value to a string.
  
  Converts a JSON value to its string representation and appends it to the internal document buffer.
  Handles all JSON value types, including nested structures, and applies formatting based on the writer's configuration.
  
  \param value The JSON value to be serialized.
  */
  void writeValue(const Value& value);

  String document_;
  bool yamlCompatibilityEnabled_{false};
  bool dropNullPlaceholders_{false};
  bool omitEndingLineFeed_{false};
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
/*!
\class StyledWriter
\brief Converts JSON values into formatted string representations with styling.

Implements the Writer interface to produce human-readable JSON output.
Manages formatting details such as indentation, line breaks, and comment placement.
Provides methods for writing various JSON elements while maintaining a consistent and visually appealing structure.
Useful for generating styled JSON documents with proper indentation and readability.
*/
class JSON_API StyledWriter : public Writer {
public:
  StyledWriter();
  ~StyledWriter() override = default;

public:
  /*!
  \brief Converts a JSON value to a styled string representation.
  
  Generates a formatted string representation of the input JSON value.
  Applies styling including indentation, line breaks, and comments to produce a human-readable output.
  
  \param root The JSON value to be converted to a styled string.
  
  \return A string containing the styled representation of the input JSON value.
  */
  String write(const Value& root) override;

private:
  /*!
  \brief Writes a JSON value to the output document.
  
  Converts a JSON value to its string representation and appends it to the output document.
  Handles all JSON value types, including nested structures, with appropriate formatting and indentation.
  
  \param value The JSON value to be written to the output document.
  */
  void writeValue(const Value& value);
  /*!
  \brief Writes a JSON array value to the output.
  
  Formats and writes a JSON array value with proper styling.
  Handles both single-line and multi-line array representations, applying appropriate indentation and separators based on the array's content and size.
  
  \param value The JSON array value to be written.
  */
  void writeArrayValue(const Value& value);
  /*!
  \brief Determines if an array should be formatted across multiple lines.
  
  Analyzes the given JSON array to decide whether it should be written on a single line or across multiple lines.
  This decision is based on the array's size, content complexity, and the presence of comments.
  
  \param value The JSON array to be evaluated for multiline formatting.
  
  \return True if the array should be formatted across multiple lines, false otherwise.
  */
  bool isMultilineArray(const Value& value);
  /*!
  \brief Appends a JSON value to the output.
  
  Adds a JSON value either to the main document or to a temporary storage for child values.
  The behavior depends on the current context, allowing for flexible formatting of inline and multi-line JSON structures.
  
  \param value The JSON value to be appended, represented as a string.
  */
  void pushValue(const String& value);
  /*!
  \brief Writes an indentation to the JSON document.
  
  Adds a newline character if necessary and appends the current indentation string to the document.
  Ensures proper formatting by checking the last character of the document and only adding indentation when appropriate.
  */
  void writeIndent();
  /*!
  \brief Writes a string with proper indentation.
  
  Applies the current indentation level before writing the given value to the document.
  Ensures that each new line in the JSON output begins with the correct level of indentation, maintaining a consistent and readable format.
  
  \param value The string to be written to the document after applying indentation.
  */
  void writeWithIndent(const String& value);
  /*!
  \brief Increases the current indentation level.
  
  Appends a specified number of spaces to the internal indentation string.
  This method is used to properly format nested JSON structures, ensuring that each level of nesting is visually distinct in the output.
  */
  void indent();
  /*!
  \brief Decreases the current indentation level.
  
  Reduces the indentation string by removing one level of indentation.
  This function is called when closing nested structures in the JSON output, such as arrays or objects, to maintain proper formatting and improve readability of the generated JSON string.
  */
  void unindent();
  /*!
  \brief Writes a comment before a JSON value.
  
  Formats and writes any comment that precedes a JSON value.
  Handles indentation and line breaks to maintain proper structure in the output document.
  This function is called before writing JSON values to preserve associated comments.
  
  \param root The JSON value whose preceding comment is to be written.
  */
  void writeCommentBeforeValue(const Value& root);
  /*!
  \brief Writes comments associated with a JSON value.
  
  Appends comments to the document string after writing a JSON value.
  Handles both same-line comments and comments on subsequent lines, preserving the structure and metadata of the original JSON.
  
  \param root The JSON value whose comments are to be written.
  */
  void writeCommentAfterValueOnSameLine(const Value& root);
  /*!
  \brief Checks if a JSON value has any associated comments.
  
  Determines whether the given JSON value has any comments attached to it.
  This includes comments before the value, after the value on the same line, or after the value on a new line.
  Used internally to decide on multi-line formatting for arrays.
  
  \param value The JSON value to check for comments.
  
  \return True if the value has any type of comment, false otherwise.
  */
  static bool hasCommentForValue(const Value& value);
  static String normalizeEOL(const String& text);

  using ChildValues = std::vector<String>;

  ChildValues childValues_;
  String document_;
  String indentString_;
  unsigned int rightMargin_{74};
  unsigned int indentSize_{3};
  bool addChildValues_{false};
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
/*!
\class StyledStreamWriter
\brief Writes JSON values to an output stream with customizable formatting and indentation.

Provides methods for writing various JSON data types to an output stream with styled formatting.
Manages indentation, line breaks, and spacing to produce readable JSON output.
Supports customization of indentation style and includes functionality for handling arrays, objects, and comments.
Offers flexibility in formatting complex JSON structures for improved readability and presentation.
*/
class JSON_API StyledStreamWriter {
public:
  /*!
  \brief Constructs a StyledStreamWriter with custom indentation.
  
  Initializes a StyledStreamWriter object with the specified indentation string.
  This constructor sets up the writer for formatting JSON output with the given indentation style, while initializing other internal state variables to their default values.
  
  \param indentation String specifying the characters to use for each level of indentation in the formatted output.
  */
  StyledStreamWriter(String indentation = "\t");
  ~StyledStreamWriter() = default;

public:
  /*!
  \brief Writes a JSON value to an output stream with styling.
  
  Formats and writes the given JSON value to the specified output stream.
  Applies indentation, line breaks, and comments according to the configured style settings.
  
  \param out The output stream to which the JSON value will be written.
  \param root The JSON value to be written to the output stream.
  */
  void write(OStream& out, const Value& root);

private:
  /*!
  \brief Writes a JSON value to the output stream.
  
  Serializes a JSON value to the output stream with appropriate formatting.
  Handles all JSON value types, including null, numbers, strings, booleans, arrays, and objects.
  Maintains proper indentation and styling for complex structures.
  
  \param value The JSON value to be written to the output stream.
  */
  void writeValue(const Value& value);
  /*!
  \brief Writes a JSON array value to the output stream.
  
  Formats and writes a JSON array to the output stream.
  Handles both single-line and multi-line array representations based on the array's content and complexity.
  Manages indentation, spacing, and comments for readable output.
  
  \param value The JSON array value to be written to the output stream.
  */
  void writeArrayValue(const Value& value);
  /*!
  \brief Determines if an array should be formatted as multi-line.
  
  Analyzes the array's content and size to decide whether it should be formatted across multiple lines.
  Considers factors such as element count, nested structures, comments, and the configured right margin.
  
  \param value The JSON array value to be analyzed.
  
  \return True if the array should be formatted as multi-line, false otherwise.
  */
  bool isMultilineArray(const Value& value);
  /*!
  \brief Writes or stores a JSON value.
  
  Outputs a JSON value to the document stream or stores it in a collection for later use.
  The behavior depends on the current writing context, allowing for flexible and styled JSON output.
  
  \param value The JSON value to be written or stored.
  */
  void pushValue(const String& value);
  /*!
  \brief Writes a newline and indentation to the output stream.
  
  Inserts a newline character followed by the current indentation string into the output stream.
  This function is used throughout the writing process to maintain proper formatting and readability of the JSON output, especially for nested structures and multi-line elements.
  */
  void writeIndent();
  /*!
  \brief Writes a value with proper indentation.
  
  Ensures the current line is properly indented before writing the provided value.
  Manages the indentation state to maintain consistent formatting of nested JSON elements.
  
  \param value The string value to be written to the output stream.
  */
  void writeWithIndent(const String& value);
  /*!
  \brief Increases the current indentation level.
  
  Appends the indentation string to the current indentation.
  Used to format JSON output with proper hierarchical structure, particularly for arrays and objects, enhancing readability of the generated JSON.
  */
  void indent();
  /*!
  \brief Decreases the current indentation level.
  
  Reduces the indentation string by removing the last indentation increment.
  Called after writing complex structures like arrays and objects to maintain proper formatting in the JSON output.
  Ensures closing delimiters align with their opening counterparts.
  */
  void unindent();
  /*!
  \brief Writes a comment before a JSON value.
  
  Checks for and writes any comment that precedes a JSON value to the output stream.
  Handles indentation and formatting to maintain the structure and readability of the JSON output.
  
  \param root The JSON value to check for a preceding comment.
  */
  void writeCommentBeforeValue(const Value& root);
  /*!
  \brief Writes comments associated with a JSON value.
  
  Appends comments to the output stream that are associated with the given JSON value.
  Handles both inline comments (on the same line) and comments that should appear on a new line after the value.
  Maintains consistent formatting and indentation for comments throughout the JSON document.
  
  \param root The JSON value whose associated comments are to be written.
  */
  void writeCommentAfterValueOnSameLine(const Value& root);
  /*!
  \brief Checks if a JSON value has any associated comments.
  
  Determines whether the given JSON value has any comments of type 'before', 'after on same line', or 'after'.
  This function is used internally to decide on formatting, particularly for array output.
  
  \param value The JSON value to check for comments.
  
  \return True if the value has any type of comment, false otherwise.
  */
  static bool hasCommentForValue(const Value& value);
  static String normalizeEOL(const String& text);

  using ChildValues = std::vector<String>;

  ChildValues childValues_;
  OStream* document_;
  String indentString_;
  unsigned int rightMargin_{74};
  String indentation_;
  bool addChildValues_ : 1;
  bool indented_ : 1;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#if defined(JSON_HAS_INT64)
/*!
\brief Converts an integer to its string representation.

Converts the given integer value to a string.
This function is part of a series of overloaded functions that handle different numeric types for JSON value conversion and serialization.

\param value The integer value to be converted to a string.

\return A string representation of the input integer value.
*/
String JSON_API valueToString(Int value);
/*!
\brief Converts an unsigned integer to a string.

Converts the given unsigned integer to its string representation.
Internally calls the overloaded version of valueToString that accepts a LargestUInt parameter.

\param value The unsigned integer to be converted.

\return The string representation of the input value.
*/
String JSON_API valueToString(UInt value);
#endif
/*!
\brief Converts an integer to its string representation.

Converts a signed integer to its string representation, handling negative values and the minimum possible value.
The resulting string is properly formatted with a leading minus sign for negative numbers.

\param value The integer value to be converted to a string.

\return A string representation of the input integer.
*/
String JSON_API valueToString(LargestInt value);
/*!
\brief Converts an unsigned integer to a string.

Converts a LargestUInt value to its string representation.
Uses an internal buffer for the conversion process.

\param value The unsigned integer to be converted to a string.

\return A String object containing the string representation of the input value.
*/
String JSON_API valueToString(LargestUInt value);
/*!
\brief Converts a double value to a string with specified precision.

Converts a double value to a string representation.
Allows control over the precision and type of precision used in the conversion.

\param value The double value to be converted to a string.
\param precision The number of decimal places or significant digits to include in the output string.
\param precisionType Specifies whether the precision refers to decimal places or significant digits.

\return A string representation of the input double value.
*/
String JSON_API valueToString(
    double value, unsigned int precision = Value::defaultRealPrecision,
    PrecisionType precisionType = PrecisionType::significantDigits);
/*!
\brief Converts a boolean value to its string representation.

Converts a boolean value to its corresponding string representation.
Returns "true" for true and "false" for false.

\param value The boolean value to convert.

\return A string representation of the boolean value.
*/
String JSON_API valueToString(bool value);
/*!
\brief Converts a C-string to a quoted JSON string.

Creates a quoted string representation of the input, suitable for use as a JSON object key.
Delegates the actual conversion to valueToQuotedStringN function.

\param value The C-string to be converted to a quoted JSON string.

\return A String object containing the input value enclosed in double quotes, with proper JSON escaping applied.
*/
String JSON_API valueToQuotedString(const char* value);
/*!
\brief Converts a character array to a quoted string.

Wraps the given character array in quotes to create a valid JSON string representation.
Delegates the actual conversion to the valueToQuotedStringN function.

\param value Pointer to the character array to be converted.
\param length Number of characters in the array to process.

\return A String object containing the quoted representation of the input.
*/
String JSON_API valueToQuotedString(const char* value, size_t length);

JSON_API OStream& operator<<(OStream&, const Value& root);

} // namespace Json

#pragma pack(pop)

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif

#endif
