/*
 * Cross-platform byte-swap functions for the D-Star wire format.
 *
 * D-Star protocol fields are little-endian on the wire.  This header provides
 * htole16/le16toh/htole32/le32toh/htobe32/be32toh using the appropriate
 * platform API (POSIX <endian.h> on Linux, OSByteOrder on macOS, and
 * _byteswap intrinsics on Windows x86).  Include this wherever protocol
 * buffers are serialised or deserialised.
 */

#ifndef EndianCompat_H
#define EndianCompat_H

#if defined(_WIN32)
#include <cstdlib>
// Windows: use _byteswap intrinsics
static inline uint16_t htole16(uint16_t x) { return x; } // x86 is little-endian
static inline uint16_t le16toh(uint16_t x) { return x; }
static inline uint32_t htole32(uint32_t x) { return x; }
static inline uint32_t le32toh(uint32_t x) { return x; }
static inline uint32_t htobe32(uint32_t x) { return _byteswap_ulong(x); }
static inline uint32_t be32toh(uint32_t x) { return _byteswap_ulong(x); }
#elif defined(__APPLE__)
#include <libkern/OSByteOrder.h>
#define htole16(x) OSSwapHostToLittleInt16(x)
#define le16toh(x) OSSwapLittleToHostInt16(x)
#define htole32(x) OSSwapHostToLittleInt32(x)
#define le32toh(x) OSSwapLittleToHostInt32(x)
#define htobe32(x) OSSwapHostToBigInt32(x)
#define be32toh(x) OSSwapBigToHostInt32(x)
#else
#include <endian.h>
#endif

#endif
