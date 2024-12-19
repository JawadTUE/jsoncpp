namespace Json {

/*!
Initializes the iterator with a null internal iterator, creating an uninitialized state suitable for placeholder use or later assignment.
*/
ValueIteratorBase::ValueIteratorBase() : current_() {}

/*!
Initializes the iterator with the provided object iterator, setting the current position and marking it as non-null.
This constructor is primarily used internally for creating iterators over JSON objects.
*/
ValueIteratorBase::ValueIteratorBase(
    const Value::ObjectValues::iterator& current)
    : current_(current), isNull_(false) {}

/*!
Returns a reference to the Value object at the current iterator position by accessing the second element of the key-value pair.
*/
Value& ValueIteratorBase::deref() { return current_->second; }
/*!
Returns a constant reference to the value of the current element pointed to by the iterator.
This function provides access to the value part of the key-value pair in a JSON object.
*/
const Value& ValueIteratorBase::deref() const { return current_->second; }

/*!
Increments the internal iterator to point to the next element in the JSON structure.
*/
void ValueIteratorBase::increment() { ++current_; }

/*!
Decrements the internal iterator, moving it one position backwards in the JSON structure.
This simple operation supports reverse iteration through JSON objects or arrays.
*/
void ValueIteratorBase::decrement() { --current_; }

ValueIteratorBase::difference_type
/*!
Calculates the number of elements between two iterators by iterating from the current position to the other iterator's position.
Returns zero if both iterators are null.
*/
ValueIteratorBase::computeDistance(const SelfType& other) const {
  if (isNull_ && other.isNull_) {
    return 0;
  }

  difference_type myDistance = 0;
  for (Value::ObjectValues::iterator it = current_; it != other.current_;
       ++it) {
    ++myDistance;
  }
  return myDistance;
}

/*!
Compares this iterator with another for equality by checking if both are null or if their current positions match.
Handles null iterators as a special case.
*/
bool ValueIteratorBase::isEqual(const SelfType& other) const {
  if (isNull_) {
    return other.isNull_;
  }
  return current_ == other.current_;
}

/*!
Copies the internal state of another iterator, assigning its current position and null status to this iterator.
*/
void ValueIteratorBase::copy(const SelfType& other) {
  current_ = other.current_;
  isNull_ = other.isNull_;
}

/*!
Retrieves the key of the current JSON element, creating a new Value object for string keys or returning a Value with the index for numeric keys.
Handles static strings and dynamic strings differently for efficiency.
*/
Value ValueIteratorBase::key() const {
  const Value::CZString czstring = (*current_).first;
  if (czstring.data()) {
    if (czstring.isStaticString())
      return Value(StaticString(czstring.data()));
    return Value(czstring.data(), czstring.data() + czstring.length());
  }
  return Value(czstring.index());
}

/*!
Retrieves the index of the current element in the JSON object.
Returns the numeric index for elements with numeric keys, or -1 (as UInt) for elements with string keys.
*/
UInt ValueIteratorBase::index() const {
  const Value::CZString czstring = (*current_).first;
  if (!czstring.data())
    return czstring.index();
  return Value::UInt(-1);
}

/*!
Retrieves the name of the current member in the JSON object iteration.
Uses the internal memberName function to obtain the key, then constructs and returns a String object from the retrieved key.
*/
String ValueIteratorBase::name() const {
  char const* keey;
  char const* end;
  keey = memberName(&end);
  if (!keey)
    return String();
  return String(keey, end);
}

/*!
Retrieves the name of the current member in the JSON object iteration.
Returns a pointer to the member's key as a C-style string, or an empty string if the key is null.
*/
char const* ValueIteratorBase::memberName() const {
  const char* cname = (*current_).first.data();
  return cname ? cname : "";
}

char const* ValueIteratorBase::memberName(char const** end) const {
  const char* cname = (*current_).first.data();
  if (!cname) {
    *end = nullptr;
    return nullptr;
  }
  *end = cname + (*current_).first.length();
  return cname;
}

ValueConstIterator::ValueConstIterator() = default;

/*!
Initializes a constant iterator for a JSON Value object using the provided object iterator, enabling read-only traversal of JSON elements.
*/
ValueConstIterator::ValueConstIterator(
    const Value::ObjectValues::iterator& current)
    : ValueIteratorBase(current) {}

/*!
Constructs a constant iterator by copying the state of a non-constant ValueIterator, enabling read-only access to JSON elements.
*/
ValueConstIterator::ValueConstIterator(ValueIterator const& other)
    : ValueIteratorBase(other) {}

ValueConstIterator&
ValueConstIterator::operator=(const ValueIteratorBase& other) {
  copy(other);
  return *this;
}

ValueIterator::ValueIterator() = default;

/*!
Initializes the ValueIterator using an ObjectValues iterator, allowing it to point to a specific position within a JSON object's key-value pairs.
Delegates the initialization to the base class ValueIteratorBase.
*/
ValueIterator::ValueIterator(const Value::ObjectValues::iterator& current)
    : ValueIteratorBase(current) {}

/*!
Initializes the base class with the provided const iterator and immediately throws a runtime error.
This constructor prevents the conversion from a const iterator to a non-const iterator.
*/
ValueIterator::ValueIterator(const ValueConstIterator& other)
    : ValueIteratorBase(other) {
  throwRuntimeError("ConstIterator to Iterator should never be allowed.");
}

ValueIterator::ValueIterator(const ValueIterator& other) = default;

ValueIterator& ValueIterator::operator=(const SelfType& other) {
  copy(other);
  return *this;
}

} // namespace Json
