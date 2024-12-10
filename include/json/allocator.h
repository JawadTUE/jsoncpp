#ifndef JSON_ALLOCATOR_H_INCLUDED
#define JSON_ALLOCATOR_H_INCLUDED

#include <algorithm>
#include <cstring>
#include <memory>

#pragma pack(push)
#pragma pack()

namespace Json {
template <typename T> class SecureAllocator {
public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  pointer allocate(size_type n) {
    return static_cast<pointer>(::operator new(n * sizeof(T)));
  }

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

  template <typename... Args> void construct(pointer p, Args&&... args) {
    ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
  }

  size_type max_size() const { return size_t(-1) / sizeof(T); }

  pointer address(reference x) const { return std::addressof(x); }

  const_pointer address(const_reference x) const { return std::addressof(x); }

  void destroy(pointer p) { p->~T(); }

  SecureAllocator() {}
  template <typename U> SecureAllocator(const SecureAllocator<U>&) {}
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
