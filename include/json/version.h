#ifndef JSON_VERSION_H_INCLUDED
#define JSON_VERSION_H_INCLUDED

#define JSONCPP_VERSION_STRING "1.9.7"
#define JSONCPP_VERSION_MAJOR 1
#define JSONCPP_VERSION_MINOR 9
#define JSONCPP_VERSION_PATCH 7
#define JSONCPP_VERSION_QUALIFIER
#define JSONCPP_VERSION_HEXA                                                   \
  ((JSONCPP_VERSION_MAJOR << 24) | (JSONCPP_VERSION_MINOR << 16) |             \
   (JSONCPP_VERSION_PATCH << 8))

#if !defined(JSONCPP_USE_SECURE_MEMORY)
#define JSONCPP_USE_SECURE_MEMORY 0
#endif

#endif
