#ifndef DSTDINT__H
#define DSTDINT__H
#if _MSC_VER

#include <sys/types.h>

#if 1
typedef __int8              int8_t;
typedef unsigned __int8     uint8_t;
typedef __int16             int16_t;
typedef unsigned __int16    uint16_t;
typedef __int32             int32_t;
typedef unsigned __int32    uint32_t;
#else
//
// for compatibility with LLVM
//
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
typedef short               int16_t;
typedef unsigned short      uint16_t;
typedef signed char         int8_t;
typedef unsigned char       uint8_t;
#endif

typedef __int64             int64_t;
typedef unsigned __int64    uint64_t;
typedef __int64             intmax_t;

#endif
#endif // DSTDINT__H
