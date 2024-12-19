#ifndef JSON_READER_H_INCLUDED
#define JSON_READER_H_INCLUDED

#if !defined(JSON_IS_AMALGAMATION)
#include "json_features.h"
#include "value.h"
#endif
#include <deque>
#include <iosfwd>
#include <istream>
#include <stack>
#include <string>

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

#pragma pack(push)
#pragma pack()

namespace Json {

/*!
\class Reader
\brief Parses and interprets JSON-formatted data.

Provides functionality to read JSON data from various sources, including strings and input streams.
Populates a Value object with the parsed data, offering error handling capabilities and support for features like comment collection.
Allows customization of parsing behavior through its Features member.
*/
class JSON_API Reader {
public:
  using Char = char;
  using Location = const Char*;

  /*!
  \class StructuredError
  \brief Represents a structured error encountered during JSON processing.
  
  Encapsulates error information for JSON parsing or processing operations.
  Contains the error's location within the JSON data and a descriptive message.
  Facilitates precise error reporting and handling in JSON-related tasks.
  */
  struct StructuredError {
    ptrdiff_t offset_start;
    ptrdiff_t offset_limit;
    String message;
  };

  /*!
  \brief Constructs a JSON reader with default features.
  
  Initializes a new Reader instance with all parsing features enabled.
  This constructor sets up the reader to use the full set of available parsing options, allowing for maximum flexibility in JSON interpretation.
  */
  Reader();

  /*!
  \brief Initializes a JSON reader with specified features.
  
  Creates a new Reader instance with custom parsing options.
  The provided features determine the behavior of the JSON parsing process, such as allowing comments or controlling numeric precision.
  
  \param features Configuration options that control the parsing behavior of the Reader.
  */
  Reader(const Features& features);

  /*!
  \brief Parses JSON data from a string.
  
  Reads and interprets JSON-formatted data from a string input.
  Populates a Value object with the parsed content, optionally collecting comments during the process.
  
  \param document The string containing JSON data to be parsed.
  \param root The Value object to be populated with the parsed JSON data.
  \param collectComments Flag indicating whether to collect comments during parsing.
  
  \return True if parsing was successful, false otherwise.
  */
  bool parse(const std::string& document, Value& root,
             bool collectComments = true);

  /*!
  \brief Parses JSON data from a character range.
  
  Reads and interprets JSON-formatted data from the given character range.
  Populates the provided Value object with the parsed data, handling comments if specified.
  Applies strict root validation when enabled in the reader's features.
  
  \param beginDoc Pointer to the beginning of the JSON document.
  \param endDoc Pointer to the end of the JSON document.
  \param root Value object to be populated with the parsed JSON data.
  \param collectComments Flag indicating whether to collect comments during parsing.
  
  \return True if parsing was successful, false otherwise.
  */
  bool parse(const char* beginDoc, const char* endDoc, Value& root,
             bool collectComments = true);

  /*!
  \brief Parses JSON data from an input stream.
  
  Reads JSON-formatted data from the provided input stream and populates a Value object with the parsed content.
  Supports optional comment collection during parsing.
  
  \param is The input stream containing JSON data to be parsed.
  \param root The Value object to be populated with the parsed JSON data.
  \param collectComments Flag indicating whether to collect comments during parsing.
  
  \return True if parsing was successful, false otherwise.
  */
  bool parse(IStream& is, Value& root, bool collectComments = true);

  JSONCPP_DEPRECATED("Use getFormattedErrorMessages() instead.")
  String getFormatedErrorMessages() const;

  /*!
  \brief Retrieves formatted error messages.
  
  Generates a formatted string containing all error messages collected during JSON parsing.
  Each error message includes the location (line and column) of the error and the error description.
  For errors with additional details, it also includes a reference to the location of those details.
  
  \return A string containing all formatted error messages, with each error on a separate line.
  */
  String getFormattedErrorMessages() const;

  /*!
  \brief Retrieves structured error information.
  
  Converts internal error data into a vector of StructuredError objects.
  Each StructuredError contains the error's start and end offsets, as well as the error message.
  
  \return A vector of StructuredError objects, each representing a parsing error with its location and message.
  */
  std::vector<StructuredError> getStructuredErrors() const;

  /*!
  \brief Adds an error to the internal error list.
  
  Records a parsing error with associated location information.
  The error is added to an internal list for later retrieval and reporting.
  
  \param value A JSON Value object containing offset information for the error location.
  \param message The error message to be associated with this error.
  
  \return True if the error was successfully added, false if the offset information is invalid.
  */
  bool pushError(const Value& value, const String& message);

  /*!
  \brief Adds an error to the internal error list.
  
  Constructs an error token and information based on the provided parameters.
  Adds this error to the internal list of errors if the offsets are valid.
  
  \param value The JSON value associated with the error, used for offset information.
  \param message The error message to be associated with this error.
  \param extra Additional JSON value providing extra context for the error.
  
  \return True if the error was successfully added, false if the offsets were invalid.
  */
  bool pushError(const Value& value, const String& message, const Value& extra);

  /*!
  \brief Checks if parsing was successful.
  
  Indicates whether the JSON parsing operation completed without errors.
  This function can be used to verify the success of a parsing operation after it has been performed.
  
  \return true if no errors occurred during parsing, false otherwise.
  */
  bool good() const;

private:
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
    tokenArraySeparator,
    tokenMemberSeparator,
    tokenComment,
    tokenError
  };

  /*!
  \class Token
  \brief Represents a single token in JSON parsing.
  
  Encapsulates a token's type and location within the input JSON text.
  Contains public members for storing the token's type and its start and end positions in the source.
  Facilitates efficient token handling during JSON parsing and processing.
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
  
  Encapsulates details about parsing errors, including the token where the error occurred, an error message, and the location within the JSON document.
  Provides a structured way to capture and report parsing issues for debugging and error handling purposes.
  */
  class ErrorInfo {
  public:
    Token token_;
    String message_;
    Location extra_;
  };

  using Errors = std::deque<ErrorInfo>;

  /*!
  \brief Reads and identifies the next JSON token.
  
  Parses the next token from the input stream, identifying its type and content.
  Handles various JSON elements including delimiters, values, and separators.
  Serves as a core component in the JSON parsing process.
  
  \param token The Token object to be populated with the parsed token's information, including its type and position in the input stream.
  
  \return true if a valid token was read, false if an error occurred or an invalid token was encountered.
  */
  bool readToken(Token& token);
  /*!
  \brief Reads a token while skipping comments.
  
  Reads the next token from the input stream, ignoring any comments encountered if comments are allowed.
  This function is crucial for parsing JSON data with optional comment support.
  
  \param token The Token object to be filled with the read token's information.
  
  \return True if a token was successfully read, false otherwise.
  */
  bool readTokenSkippingComments(Token& token);
  /*!
  \brief Skips whitespace characters in the input.
  
  Advances the current parsing position past any whitespace characters (space, tab, carriage return, or newline).
  This function is used throughout the parsing process to ignore insignificant whitespace between JSON elements, ensuring proper interpretation of the JSON structure.
  */
  void skipSpaces();
  /*!
  \brief Matches a specific character pattern in the input stream.
  
  Compares a sequence of characters in the current parsing position with a given pattern.
  Advances the current position if the pattern matches.
  Used for validating and consuming JSON literals like 'true', 'false', and 'null'.
  
  \param pattern The character sequence to match against the current input position.
  \param patternLength The length of the pattern to match.
  
  \return True if the pattern matches and the current position was advanced, false otherwise.
  */
  bool match(const Char* pattern, int patternLength);
  /*!
  \brief Reads and processes a comment in the JSON input.
  
  Identifies and handles C-style or C++-style comments in the JSON input.
  If comment collection is enabled, it adds the comment to the appropriate location relative to the last parsed value.
  
  \return True if a comment was successfully read and processed, false otherwise.
  */
  bool readComment();
  /*!
  \brief Reads and consumes a C-style comment.
  
  Advances the current position in the input stream until the end of a C-style comment ('* /') is found.
  This function is used internally to skip C-style comments during JSON parsing, allowing for non-standard comment handling in JSON-like formats.
  
  \return True if the comment was successfully consumed, false otherwise.
  */
  bool readCStyleComment();
  /*!
  \brief Processes a C++-style comment.
  
  Reads and skips characters until the end of the current line, effectively ignoring a C++-style comment.
  Handles both '\n' and '\r\n' line endings.
  
  \return Always true, indicating successful processing of the comment.
  */
  bool readCppStyleComment();
  /*!
  \brief Reads a JSON string token.
  
  Processes characters in the input stream until the closing double quote of a JSON string is encountered.
  Handles escape characters within the string.
  This function is called when a string token is detected during JSON parsing.
  
  \return True if the string was successfully read (closing quote found), false otherwise.
  */
  bool readString();
  /*!
  \brief Parses a numeric value from the input stream.
  
  Advances the current position in the input stream to read a complete number token.
  Handles integer and floating-point numbers, including those with exponents.
  Does not perform conversion; only identifies the full extent of the numeric string.
  */
  void readNumber();
  /*!
  \brief Parses and constructs a JSON value.
  
  Reads the next token from the input stream and constructs the corresponding JSON value.
  Handles various JSON data types including objects, arrays, numbers, strings, booleans, and null values.
  Manages error handling, comment collection, and offset tracking for each parsed value.
  
  \return True if the value was successfully parsed, false otherwise.
  */
  bool readValue();
  /*!
  \brief Parses a JSON object.
  
  Reads and constructs a JSON object from the input stream.
  Handles key-value pairs, nested structures, and potential parsing errors.
  Supports both string and numeric keys based on configuration.
  
  \param token The initial token indicating the start of the object.
  
  \return True if the object was successfully parsed, false otherwise.
  */
  bool readObject(Token& token);
  /*!
  \brief Parses a JSON array.
  
  Reads and constructs a JSON array from the input stream.
  Handles nested elements, separators, and the array's closing bracket.
  Populates the current value with the parsed array data.
  
  \param token The token representing the start of the array.
  
  \return True if the array was successfully parsed, false otherwise.
  */
  bool readArray(Token& token);
  /*!
  \brief Decodes a numeric token into a JSON value.
  
  Parses a numeric token, converts it into a JSON Value object, and updates the current value in the parsing context.
  Sets the offset information for the parsed number.
  
  \param token The token representing the numeric value to be decoded.
  
  \return true if the number was successfully decoded, false otherwise.
  */
  bool decodeNumber(Token& token);
  /*!
  \brief Decodes a numeric JSON value.
  
  Attempts to parse a numeric token into an integer or floating-point value.
  If the number can be represented as an integer, it is stored as such; otherwise, it falls back to floating-point representation.
  
  \param token The token containing the numeric string to be decoded.
  \param decoded The Value object where the decoded number will be stored.
  
  \return True if the number was successfully decoded, false otherwise.
  */
  bool decodeNumber(Token& token, Value& decoded);
  /*!
  \brief Decodes a JSON string token.
  
  Parses and decodes a JSON string token, handling any necessary character escaping or Unicode processing.
  Updates the current Value object with the decoded string and sets its offset information.
  
  \param token The token representing the JSON string to be decoded.
  
  \return true if the string was successfully decoded, false otherwise.
  */
  bool decodeString(Token& token);
  /*!
  \brief Decodes a JSON string token.
  
  Processes a JSON string token, handling escape sequences and Unicode code points.
  Converts the token into a decoded string, expanding escape sequences into their corresponding characters.
  
  \param token The JSON string token to be decoded.
  \param decoded The string where the decoded result will be stored.
  
  \return True if decoding was successful, false if an error occurred during decoding.
  */
  bool decodeString(Token& token, String& decoded);
  /*!
  \brief Decodes a double-precision floating-point number from a token.
  
  Parses the token as a double-precision floating-point number and updates the current value in the JSON structure.
  This function is used as a fallback for handling large integers or floating-point numbers that cannot be represented as standard integers.
  
  \param token The token containing the string representation of the number to be decoded.
  
  \return true if the number was successfully decoded, false otherwise.
  */
  bool decodeDouble(Token& token);
  /*!
  \brief Decodes a double value from a token.
  
  Attempts to parse a double value from the given token.
  Handles special cases such as infinity and performs error checking.
  If successful, stores the parsed value in the decoded parameter.
  
  \param token The token containing the string representation of the double value to be parsed.
  \param decoded The Value object where the parsed double will be stored if decoding is successful.
  
  \return True if the double was successfully decoded, false otherwise.
  */
  bool decodeDouble(Token& token, Value& decoded);
  /*!
  \brief Decodes a Unicode code point from a JSON string.
  
  Parses and decodes a Unicode code point, including handling of surrogate pairs for characters outside the Basic Multilingual Plane.
  Advances the current position in the string as it processes the escape sequence.
  
  \param token The token being processed
  \param current The current position in the JSON string
  \param end The end position of the JSON string
  \param unicode The decoded Unicode code point
  
  \return True if the Unicode code point was successfully decoded, false otherwise
  */
  bool decodeUnicodeCodePoint(Token& token, Location& current, Location end,
                              unsigned int& unicode);
  /*!
  \brief Decodes a Unicode escape sequence.
  
  Parses a four-digit hexadecimal Unicode escape sequence and converts it to its corresponding integer value.
  This function is crucial for handling Unicode characters in JSON strings, especially for characters outside the ASCII range.
  
  \param token The token being processed, used for error reporting.
  \param current Iterator pointing to the current position in the input stream.
  \param end Iterator pointing to the end of the input stream.
  \param unicode Output parameter to store the decoded Unicode code point.
  
  \return True if the Unicode escape sequence was successfully decoded, false otherwise.
  */
  bool decodeUnicodeEscapeSequence(Token& token, Location& current,
                                   Location end, unsigned int& unicode);
  /*!
  \brief Adds an error to the internal error collection.
  
  Records a parsing error with the provided message, token, and location information.
  Used throughout the parsing process to handle and document various error conditions.
  
  \param message The error message describing the issue encountered.
  \param token The problematic token associated with the error.
  \param extra Additional location information related to the error.
  
  \return Always false, indicating an error has occurred.
  */
  bool addError(const String& message, Token& token, Location extra = nullptr);
  /*!
  \brief Attempts to recover from a parsing error.
  
  Skips tokens in the input stream until a specified token type or the end of the stream is reached.
  This function is used as an error recovery mechanism during JSON parsing, allowing the parser to continue processing after encountering an error in array or object structures.
  
  \param skipUntilToken The token type to search for when skipping. Parsing will resume when this token is found or at the end of the stream.
  
  \return Always false, indicating that an error occurred and recovery was attempted.
  */
  bool recoverFromError(TokenType skipUntilToken);
  /*!
  \brief Adds an error message and attempts to recover from the error.
  
  Logs an error message associated with the current token and attempts to recover by skipping tokens until a specified token type is reached.
  This function is used to handle parsing errors while continuing to process the JSON input.
  
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
  
  Provides access to the top element of the nodes stack, representing the current JSON value under construction or modification.
  This function is essential for the internal parsing process, allowing efficient manipulation of the JSON structure being built.
  
  \return A reference to the current Value object at the top of the nodes stack.
  */
  Value& currentValue();
  /*!
  \brief Retrieves the next character from the input stream.
  
  Advances the current position in the input stream and returns the character at that position.
  This function is essential for sequential parsing of JSON data, allowing the parser to process the input character by character.
  
  \return The next character in the input stream, or 0 if the end of the stream has been reached.
  */
  Char getNextChar();
  /*!
  \brief Calculates line and column numbers for a given location in the JSON input.
  
  Converts a location within the JSON input to corresponding line and column numbers.
  This function is used internally to provide precise error location information for JSON parsing issues.
  
  \param location The position within the JSON input to be converted.
  \param line The calculated line number for the given location.
  \param column The calculated column number for the given location.
  */
  void getLocationLineAndColumn(Location location, int& line,
                                int& column) const;
  /*!
  \brief Converts a location to a string representation.
  
  Generates a human-readable string describing the line and column of a given location in the JSON document.
  This is useful for error reporting and debugging.
  
  \param location The location in the JSON document to be converted.
  
  \return A string containing the line and column information in the format 'Line X, Column Y'.
  */
  String getLocationLineAndColumn(Location location) const;
  /*!
  \brief Adds a comment to the JSON structure.
  
  Normalizes and stores a comment encountered during JSON parsing.
  Depending on the placement, attaches the comment to the last parsed value or stores it for later use.
  Only functions when comment collection is enabled.
  
  \param begin The starting location of the comment in the input.
  \param end The ending location of the comment in the input.
  \param placement The position of the comment relative to JSON values (before, after, or on the same line).
  */
  void addComment(Location begin, Location end, CommentPlacement placement);

  /*!
  \brief Checks for newline characters in a given range.
  
  Scans the specified range of characters for the presence of newline characters ('\n' or '\r').
  Used internally to determine the placement of comments in the JSON structure.
  
  \param begin The starting location of the range to check.
  \param end The ending location of the range to check.
  
  \return True if a newline character is found within the range, false otherwise.
  */
  static bool containsNewLine(Location begin, Location end);
  /*!
  \brief Normalizes line endings in a character range.
  
  Converts Windows-style line endings (CRLF) to Unix-style line endings (LF) within the specified range of characters.
  Ensures consistent handling of line breaks across different platforms and input sources.
  
  \param begin Iterator pointing to the start of the character range to normalize.
  \param end Iterator pointing to the end of the character range to normalize.
  
  \return A string with normalized line endings.
  */
  static String normalizeEOL(Location begin, Location end);

  using Nodes = std::stack<Value*>;
  Nodes nodes_;
  Errors errors_;
  String document_;
  Location begin_{};
  Location end_{};
  Location current_{};
  Location lastValueEnd_{};
  Value* lastValue_{};
  String commentsBefore_;
  Features features_;
  bool collectComments_{};
};
/*!
\class CharReader
\brief Reads and parses JSON documents from character streams.

Provides functionality for parsing JSON data into structured Value objects.
Offers error handling capabilities, including retrieval of structured error information.
Utilizes a flexible implementation strategy through the pimpl idiom and includes a Factory for creating instances.
Designed for efficient and robust JSON parsing in C++ applications.
*/
class JSON_API CharReader {
public:
  /*!
  \class StructuredError
  \brief Represents structured error information for JSON parsing.
  
  Encapsulates details about errors encountered during JSON parsing operations.
  Provides a structured way to report parsing errors, including the error location within the input and a descriptive message.
  This allows for more precise error handling and reporting in JSON processing applications.
  */
  struct JSON_API StructuredError {
    ptrdiff_t offset_start;
    ptrdiff_t offset_limit;
    String message;
  };

  virtual ~CharReader() = default;
  /*!
  \brief Parses a JSON document.
  
  Reads and parses a JSON document from the given character range.
  Constructs a Value object representing the parsed JSON structure.
  Reports any parsing errors encountered during the process.
  
  \param beginDoc Pointer to the beginning of the JSON document.
  \param endDoc Pointer to the end of the JSON document.
  \param root Pointer to a Value object where the parsed JSON structure will be stored.
  \param errs Pointer to a String object where any error messages will be written.
  
  \return True if parsing was successful, false otherwise.
  */
  virtual bool parse(char const* beginDoc, char const* endDoc, Value* root,
                     String* errs);

  /*!
  \brief Retrieves structured error information.
  
  Provides access to detailed, structured error information collected during JSON parsing.
  This method allows for more granular error handling and reporting compared to simple error messages.
  
  \return A collection of structured error objects containing detailed information about parsing errors.
  */
  std::vector<StructuredError> getStructuredErrors() const;

  /*!
  \class Factory
  \brief Serves as an abstract factory for creating CharReader objects.
  
  Defines an interface for creating CharReader instances, allowing for flexibility in JSON parsing strategies.
  Implements the factory method pattern with a single pure virtual method, newCharReader(), which is responsible for instantiating and returning new CharReader objects.
  This design enables the creation of different CharReader implementations while maintaining a common interface for their instantiation.
  */
  class JSON_API Factory {
  public:
    virtual ~Factory() = default;
    /*!
    \brief Creates a new CharReader instance.
    
    Instantiates and returns a new CharReader object.
    This virtual method allows for the creation of different CharReader implementations with potentially varying configurations, providing flexibility in JSON parsing strategies.
    
    \return A pointer to a newly created CharReader object. The caller is responsible for managing the memory of the returned object.
    */
    virtual CharReader* newCharReader() const = 0;
  };

protected:
  /*!
  \class Impl
  \brief Defines an interface for parsing JSON documents.
  
  Provides an abstract base for JSON parsing implementations.
  Declares methods for parsing JSON from character ranges, storing results in Value objects, and reporting errors.
  Offers functionality to retrieve structured error information, enabling detailed error reporting in JSON parsing operations.
  Designed to be extended by concrete implementations for flexible and efficient JSON processing.
  */
  class Impl {
  public:
    virtual ~Impl() = default;
    /*!
    \brief Parses a JSON document.
    
    Reads a JSON document from a character range and populates a Value object with the parsed data.
    Reports any parsing errors encountered during the process.
    
    \param beginDoc Pointer to the beginning of the JSON document.
    \param endDoc Pointer to the end of the JSON document.
    \param root Pointer to the Value object where the parsed JSON data will be stored.
    \param errs Pointer to a String object where any error messages will be written.
    
    \return True if parsing was successful, false otherwise.
    */
    virtual bool parse(char const* beginDoc, char const* endDoc, Value* root,
                       String* errs) = 0;
    /*!
    \brief Retrieves structured error information from the JSON parsing process.
    
    Provides detailed error information collected during JSON parsing.
    This function allows access to structured error data, enabling more precise error handling and reporting in client code.
    
    \return A vector of StructuredError objects, each containing detailed information about a parsing error encountered.
    */
    virtual std::vector<StructuredError> getStructuredErrors() const = 0;
  };

  /*!
  \brief Constructs a CharReader with a given implementation.
  
  Initializes a CharReader object with a custom implementation.
  Takes ownership of the provided Impl object, allowing for flexible and extensible parsing strategies.
  
  \param impl A unique pointer to the CharReader implementation. Ownership is transferred to the CharReader object.
  */
  explicit CharReader(std::unique_ptr<Impl> impl) : _impl(std::move(impl)) {}

private:
  std::unique_ptr<Impl> _impl;
};
/*!
\class CharReaderBuilder
\brief Serves as a factory for creating customizable CharReader objects.

Provides a flexible way to configure JSON parsing options.
Allows fine-tuning of settings such as comment handling, trailing comma acceptance, and numeric key allowance.
Offers methods for setting default configurations, enforcing strict parsing rules, or adhering to ECMA-404 standards, giving developers precise control over the JSON parsing process.
*/
class JSON_API CharReaderBuilder : public CharReader::Factory {
public:
  Json::Value settings_;

  /*!
  \brief Constructs a CharReaderBuilder with default settings.
  
  Initializes a new CharReaderBuilder instance and sets up default configuration for JSON parsing.
  The default settings are applied to the internal settings_ member, preparing the builder for customization or immediate use in creating CharReader objects.
  */
  CharReaderBuilder();
  ~CharReaderBuilder() override;

  /*!
  \brief Creates a new CharReader instance.
  
  Constructs a CharReader with the current settings.
  The created reader applies the parsing options configured in this CharReaderBuilder, such as comment handling, trailing comma allowance, and numeric key support.
  
  \return A pointer to a newly created CharReader object. The caller is responsible for managing the memory of this object.
  */
  CharReader* newCharReader() const override;

  /*!
  \brief Validates the current settings of the CharReaderBuilder.
  
  Checks if all the keys in the settings_ member are valid according to a predefined set of allowed keys.
  If invalid keys are found, they are either reported or cause the validation to fail.
  
  \param invalid Pointer to a Json::Value object that will store any invalid settings found. If null, the function will return false on the first invalid setting encountered.
  
  \return True if all settings are valid, or if invalid is non-null and contains all invalid settings. False if an invalid setting is found and invalid is null.
  */
  bool validate(Json::Value* invalid) const;

  Value& operator[](const String& key);

  /*!
  \brief Initializes default settings for JSON parsing.
  
  Populates a Json::Value object with default configuration settings for a CharReader.
  Sets various parsing options such as comment handling, trailing comma allowance, and numeric key restrictions.
  These defaults provide a consistent starting point for JSON parsing in the library.
  
  \param settings Pointer to a Json::Value object that will be populated with default configuration settings. The object should be empty or any existing key-value pairs may be overwritten.
  */
  static void setDefaults(Json::Value* settings);
  /*!
  \brief Configures strict parsing settings.
  
  Sets various parsing options to enforce strict JSON parsing rules.
  This includes disabling comments, trailing commas, and other lenient parsing features, while enabling stricter validation checks.
  
  \param settings A pointer to a Json::Value object where the strict parsing settings will be stored.
  */
  static void strictMode(Json::Value* settings);
  /*!
  \brief Configures settings for strict ECMA-404 JSON parsing.
  
  Sets parsing options to adhere strictly to the ECMA-404 JSON standard.
  Disables all non-standard features and enforces strict JSON parsing rules.
  
  \param settings A pointer to a Json::Value object containing the configuration settings to be modified for ECMA-404 compliance.
  */
  static void ecma404Mode(Json::Value* settings);
};
/*!
\brief Parses JSON from an input stream.

Reads JSON data from an input stream, parses it, and stores the result in a Value object.
Uses a CharReader created by the provided factory for parsing.

\param fact The factory used to create a CharReader for parsing.
\param sin The input stream containing the JSON data to be parsed.
\param root Pointer to the Value object where the parsed JSON will be stored.
\param errs Pointer to a String object where any error messages will be written.

\return True if parsing was successful, false otherwise.
*/
bool JSON_API parseFromStream(CharReader::Factory const&, IStream&, Value* root,
                              String* errs);
JSON_API IStream& operator>>(IStream&, Value&);

} // namespace Json

#pragma pack(pop)

#if defined(JSONCPP_DISABLE_DLL_INTERFACE_WARNING)
#pragma warning(pop)
#endif

#endif
