#ifndef JSON_ALLOCATOR_H_INCLUDED
#define JSON_ALLOCATOR_H_INCLUDED

#include <algorithm>
#include <cstring>
#include <memory>

#pragma pack(push)
#pragma pack()

namespace Json {
/*!
\class SecureAllocator
\brief Provides secure memory management for JSON data.

Implements a custom memory allocator template with enhanced security measures for handling sensitive JSON data.
Offers standard allocator functions with additional safeguards, such as securely zeroing memory before deallocation.
Supports various platforms and ensures efficient memory operations while maintaining a secure approach to data handling.
*/
template <typename T> class SecureAllocator {
public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  /*!
  \brief Allocates memory for n objects of type T.
  
  Allocates raw memory for n objects of type T using the global operator new.
  This function is part of the allocator interface and is used internally by containers that use this allocator.
  
  \param n The number of objects for which to allocate memory.
  
  \return A pointer to the allocated memory block.
  */
  pointer allocate(size_type n) {
    return static_cast<pointer>(::operator new(n * sizeof(T)));
  }

  /*!
  \brief Deallocates memory securely.
  
  Securely deallocates memory by zeroing it before releasing.
  Uses platform-specific methods when available for secure memory clearing, falling back to a volatile fill operation if no secure method is available.
  
  \param p Pointer to the memory block to be deallocated.
  \param n Number of objects in the memory block to be deallocated.
  */
  void deallocate(pointer p, size_type n) {
#if defined(HAVE_MEMSET_S)
    memset_s(p, n * sizeof(T), 0, n * sizeof(T));
#elif defined(_WIN32)
    RtlSecureZeroMemory(p, n * sizeof(T));
#else
    std::fill_n(reinterpret_cast<volatile unsigned char*>(p), n, 0);
#endif

    ::operator delete(p);
  }

  /*!
  \brief Constructs an object at a specified location.
  
  Constructs an object of type T at the given memory location using perfect forwarding of arguments.
  This function is part of the allocator interface and is used for in-place construction of objects.
  
  \param p Pointer to the memory location where the object will be constructed.
  \param args Variable number of arguments to be forwarded to the object's constructor.
  */
  template <typename... Args> void construct(pointer p, Args&&... args) {
    ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
  }

  /*!
  \brief Returns the maximum number of elements that can be allocated.
  
  Calculates the maximum number of elements of type T that can be allocated by this allocator.
  This is determined by dividing the maximum size_t value by the size of T.
  
  \return The maximum number of elements that can be allocated.
  */
  size_type max_size() const { return size_t(-1) / sizeof(T); }

  /*!
  \brief Obtains the address of an object.
  
  Returns a pointer to the memory location of the given object.
  This function is a wrapper around std::addressof, ensuring compatibility with the allocator interface.
  
  \param x The object whose address is to be obtained.
  
  \return A pointer to the memory location of the given object.
  */
  pointer address(reference x) const { return std::addressof(x); }

  /*!
  \brief Returns the address of the given object.
  
  Obtains the memory address of the specified object.
  This is a standard allocator function that provides the address of a const-qualified object.
  
  \param x The const reference to the object whose address is to be obtained.
  
  \return A const pointer to the memory location of the given object.
  */
  const_pointer address(const_reference x) const { return std::addressof(x); }

  /*!
  \brief Destroys an object at the specified memory location.
  
  Calls the destructor of the object pointed to by p without deallocating the memory.
  This is part of the allocator interface and is used in container operations.
  
  \param p Pointer to the object to be destroyed.
  */
  void destroy(pointer p) { p->~T(); }

  /*!
  \brief Constructs a default SecureAllocator object.
  
  Creates a new instance of the SecureAllocator.
  This default constructor initializes the allocator without any specific parameters, setting up the secure memory management system for JSON data.
  It prepares the allocator for subsequent memory allocation and deallocation operations with enhanced security measures.
  */
  SecureAllocator() {}
  /*!
  \brief Constructs a SecureAllocator from another SecureAllocator of a different type.
  
  Creates a new SecureAllocator instance by copying from a SecureAllocator of a different type.
  This constructor enables the secure allocation mechanism to work with different data types while maintaining the same security features.
  It does not perform any additional operations, as the internal state is independent of the template parameter.
  */
  template <typename U> SecureAllocator(const SecureAllocator<U>&) {}
  /*!
  \class rebind
  \brief Provides a mechanism for allocator rebinding.
  
  Defines a type alias 'other' that represents a SecureAllocator instantiated with a different type U.
  This allows for flexible memory allocation across different data types within the JSON library, enabling the use of the same allocator template with various data structures.
  */
  template <typename U> struct rebind {
    using other = SecureAllocator<U>;
  };
};

template <typename T, typename U>
bool operator==(const SecureAllocator<T>&, const SecureAllocator<U>&) {
  return true;
}

template <typename T, typename U>
bool operator!=(const SecureAllocator<T>&, const SecureAllocator<U>&) {
  return false;
}

}

#pragma pack(pop)

#endif
