#ifndef EBUTIL_H
#define EBUTIL_H

#include <byteswap.h>

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define htole64(x) (uint64_t)(x)
#define le64toh(x) (uint64_t)(x)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define htole64(x) bswap_64(x)
#define le64toh(x) bswap_64(x)
#else
#error unknown endianness
#endif

#endif // EBUTIL_H
