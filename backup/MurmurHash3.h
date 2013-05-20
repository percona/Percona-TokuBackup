//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// The code comes from https://code.google.com/p/smhasher/wiki/MurmurHash3

#ifndef _MURMURHASH3_H_
#define _MURMURHASH3_H_

//-----------------------------------------------------------------------------
// Platform-specific functions and macros

// Microsoft Visual Studio

#if defined(_MSC_VER)

typedef unsigned char uint8_t;
typedef unsigned long uint32_t;
typedef unsigned __int64 uint64_t;

// Other compilers

#else	// defined(_MSC_VER)

#include <stdint.h>

#endif // !defined(_MSC_VER)

//-----------------------------------------------------------------------------

#define MURMUR_64_ONLY

#ifndef MURMUR_64_ONLY
void MurmurHash3_x86_32  ( const void * key, int len, uint32_t seed, void * out ) throw();

void MurmurHash3_x86_128 ( const void * key, int len, uint32_t seed, void * out ) throw();
#endif

void MurmurHash3_x64_128 ( const void * key, int len, uint32_t seed, void * out ) throw();

//-----------------------------------------------------------------------------

#endif // _MURMURHASH3_H_
