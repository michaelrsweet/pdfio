//
// MD5 functions for PDFio.
//
// Copyright © 2021-2025 by Michael R Sweet.
// Copyright © 1999 Aladdin Enterprises.  All rights reserved.
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
// L. Peter Deutsch
// ghost@aladdin.com
//

#include "pdfio-private.h"


/*
  Independent implementation of MD5 (RFC 1321).

  This code implements the MD5 Algorithm defined in RFC 1321.
  It is derived directly from the text of the RFC and not from the
  reference implementation.

  The original and principal author of md5.c is L. Peter Deutsch
  <ghost@aladdin.com>.  Other authors are noted in the change history
  that follows (in reverse chronological order):

  1999-11-04 lpd Edited comments slightly for automatic TOC extraction.
  1999-10-18 lpd Fixed typo in header comment (ansi2knr rather than md5).
  1999-05-03 lpd Original version.
 */

#define T1 0xd76aa478
#define T2 0xe8c7b756
#define T3 0x242070db
#define T4 0xc1bdceee
#define T5 0xf57c0faf
#define T6 0x4787c62a
#define T7 0xa8304613
#define T8 0xfd469501
#define T9 0x698098d8
#define T10 0x8b44f7af
#define T11 0xffff5bb1
#define T12 0x895cd7be
#define T13 0x6b901122
#define T14 0xfd987193
#define T15 0xa679438e
#define T16 0x49b40821
#define T17 0xf61e2562
#define T18 0xc040b340
#define T19 0x265e5a51
#define T20 0xe9b6c7aa
#define T21 0xd62f105d
#define T22 0x02441453
#define T23 0xd8a1e681
#define T24 0xe7d3fbc8
#define T25 0x21e1cde6
#define T26 0xc33707d6
#define T27 0xf4d50d87
#define T28 0x455a14ed
#define T29 0xa9e3e905
#define T30 0xfcefa3f8
#define T31 0x676f02d9
#define T32 0x8d2a4c8a
#define T33 0xfffa3942
#define T34 0x8771f681
#define T35 0x6d9d6122
#define T36 0xfde5380c
#define T37 0xa4beea44
#define T38 0x4bdecfa9
#define T39 0xf6bb4b60
#define T40 0xbebfbc70
#define T41 0x289b7ec6
#define T42 0xeaa127fa
#define T43 0xd4ef3085
#define T44 0x04881d05
#define T45 0xd9d4d039
#define T46 0xe6db99e5
#define T47 0x1fa27cf8
#define T48 0xc4ac5665
#define T49 0xf4292244
#define T50 0x432aff97
#define T51 0xab9423a7
#define T52 0xfc93a039
#define T53 0x655b59c3
#define T54 0x8f0ccc92
#define T55 0xffeff47d
#define T56 0x85845dd1
#define T57 0x6fa87e4f
#define T58 0xfe2ce6e0
#define T59 0xa3014314
#define T60 0x4e0811a1
#define T61 0xf7537e82
#define T62 0xbd3af235
#define T63 0x2ad7d2bb
#define T64 0xeb86d391


//
// Use the unoptimized (big-endian) implementation if we don't know the
// endian-ness of the platform.
//

#ifdef __BYTE_ORDER__
#  if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#    define ARCH_IS_BIG_ENDIAN	0	// Use little endian optimized version
#  else
#    define ARCH_IS_BIG_ENDIAN	1	// Use generic version
#  endif // __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#elif !defined(ARCH_IS_BIG_ENDIAN)
#  define ARCH_IS_BIG_ENDIAN	1	// Use generic version
#endif // !ARCH_IS_BIG_ENDIAN


//
// 'md5_process()' - Hash a block of data.
//

static void
md5_process(_pdfio_md5_t  *pms,		// I - MD5 state
            const uint8_t *data/*[64]*/)// I - Data
{
  uint32_t	a = pms->abcd[0],	// First word of state
		b = pms->abcd[1],	// Second word of state
		c = pms->abcd[2],	// Third word of state
		d = pms->abcd[3];	// Fourth word of state
  uint32_t	t;			// Temporary state


#if ARCH_IS_BIG_ENDIAN
  // On big-endian machines, we must arrange the bytes in the right
  // order.  (This also works on machines of unknown byte order.)
  uint32_t	X[16];			// Little-endian representation
  const uint8_t	*xp;			// Pointer into data
  int		i;			// Looping var

  for (i = 0, xp = data; i < 16; i ++, xp += 4)
    X[i] = xp[0] + (unsigned)(xp[1] << 8) + (unsigned)(xp[2] << 16) + (unsigned)(xp[3] << 24);

#else  /* !ARCH_IS_BIG_ENDIAN */
  // On little-endian machines, we can process properly aligned data without copying it.
  uint32_t	xbuf[16];		// Aligned buffer
  const uint32_t *X;			// Pointer to little-endian representation

  if (!((data - (const uint8_t *)0) & 3))
  {
    // data is properly aligned, use it directly...
    X = (const uint32_t *)data;
  }
  else
  {
    // data is not aligned, copy to the aligned buffer...
    memcpy(xbuf, data, 64);
    X = xbuf;
  }
#endif // ARCH_IS_BIG_ENDIAN

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

  // Round 1.
  // Let [abcd k s i] denote the operation
  //   a = b + ((a + F(b,c,d) + X[k] + T[i]) <<< s).
#define F(x, y, z)		(((x) & (y)) | (~(x) & (z)))
#define SET(a, b, c, d, k, s, Ti) t = a + F(b,c,d) + X[k] + Ti; a = ROTATE_LEFT(t, s) + b

  // Do the following 16 operations.
  SET(a, b, c, d,  0,  7,  T1);
  SET(d, a, b, c,  1, 12,  T2);
  SET(c, d, a, b,  2, 17,  T3);
  SET(b, c, d, a,  3, 22,  T4);
  SET(a, b, c, d,  4,  7,  T5);
  SET(d, a, b, c,  5, 12,  T6);
  SET(c, d, a, b,  6, 17,  T7);
  SET(b, c, d, a,  7, 22,  T8);
  SET(a, b, c, d,  8,  7,  T9);
  SET(d, a, b, c,  9, 12, T10);
  SET(c, d, a, b, 10, 17, T11);
  SET(b, c, d, a, 11, 22, T12);
  SET(a, b, c, d, 12,  7, T13);
  SET(d, a, b, c, 13, 12, T14);
  SET(c, d, a, b, 14, 17, T15);
  SET(b, c, d, a, 15, 22, T16);

#undef SET

  // Round 2.
  // Let [abcd k s i] denote the operation
  //   a = b + ((a + G(b,c,d) + X[k] + T[i]) <<< s).
#define G(x, y, z)		(((x) & (z)) | ((y) & ~(z)))
#define SET(a, b, c, d, k, s, Ti) t = a + G(b,c,d) + X[k] + Ti; a = ROTATE_LEFT(t, s) + b

  // Do the following 16 operations.
  SET(a, b, c, d,  1,  5, T17);
  SET(d, a, b, c,  6,  9, T18);
  SET(c, d, a, b, 11, 14, T19);
  SET(b, c, d, a,  0, 20, T20);
  SET(a, b, c, d,  5,  5, T21);
  SET(d, a, b, c, 10,  9, T22);
  SET(c, d, a, b, 15, 14, T23);
  SET(b, c, d, a,  4, 20, T24);
  SET(a, b, c, d,  9,  5, T25);
  SET(d, a, b, c, 14,  9, T26);
  SET(c, d, a, b,  3, 14, T27);
  SET(b, c, d, a,  8, 20, T28);
  SET(a, b, c, d, 13,  5, T29);
  SET(d, a, b, c,  2,  9, T30);
  SET(c, d, a, b,  7, 14, T31);
  SET(b, c, d, a, 12, 20, T32);

#undef SET

  // Round 3.
  // Let [abcd k s t] denote the operation
  //   a = b + ((a + H(b,c,d) + X[k] + T[i]) <<< s).
#define H(x, y, z)		((x) ^ (y) ^ (z))
#define SET(a, b, c, d, k, s, Ti) t = a + H(b,c,d) + X[k] + Ti; a = ROTATE_LEFT(t, s) + b

  // Do the following 16 operations.
  SET(a, b, c, d,  5,  4, T33);
  SET(d, a, b, c,  8, 11, T34);
  SET(c, d, a, b, 11, 16, T35);
  SET(b, c, d, a, 14, 23, T36);
  SET(a, b, c, d,  1,  4, T37);
  SET(d, a, b, c,  4, 11, T38);
  SET(c, d, a, b,  7, 16, T39);
  SET(b, c, d, a, 10, 23, T40);
  SET(a, b, c, d, 13,  4, T41);
  SET(d, a, b, c,  0, 11, T42);
  SET(c, d, a, b,  3, 16, T43);
  SET(b, c, d, a,  6, 23, T44);
  SET(a, b, c, d,  9,  4, T45);
  SET(d, a, b, c, 12, 11, T46);
  SET(c, d, a, b, 15, 16, T47);
  SET(b, c, d, a,  2, 23, T48);

#undef SET

  // Round 4.
  // Let [abcd k s t] denote the operation
  //   a = b + ((a + I(b,c,d) + X[k] + T[i]) <<< s).
#define I(x, y, z)		((y) ^ ((x) | ~(z)))
#define SET(a, b, c, d, k, s, Ti) t = a + I(b,c,d) + X[k] + Ti; a = ROTATE_LEFT(t, s) + b

  // Do the following 16 operations.
  SET(a, b, c, d,  0,  6, T49);
  SET(d, a, b, c,  7, 10, T50);
  SET(c, d, a, b, 14, 15, T51);
  SET(b, c, d, a,  5, 21, T52);
  SET(a, b, c, d, 12,  6, T53);
  SET(d, a, b, c,  3, 10, T54);
  SET(c, d, a, b, 10, 15, T55);
  SET(b, c, d, a,  1, 21, T56);
  SET(a, b, c, d,  8,  6, T57);
  SET(d, a, b, c, 15, 10, T58);
  SET(c, d, a, b,  6, 15, T59);
  SET(b, c, d, a, 13, 21, T60);
  SET(a, b, c, d,  4,  6, T61);
  SET(d, a, b, c, 11, 10, T62);
  SET(c, d, a, b,  2, 15, T63);
  SET(b, c, d, a,  9, 21, T64);

#undef SET

  // Then perform the following additions. (That is increment each of the four
  // registers by the value it had before this block was started.)
  pms->abcd[0] += a;
  pms->abcd[1] += b;
  pms->abcd[2] += c;
  pms->abcd[3] += d;
}


//
// '_pdfioCryptoMD5Init()' - Initialize an MD5 hash.
//

void
_pdfioCryptoMD5Init(_pdfio_md5_t *pms)	// I - MD5 state
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0]  = 0x67452301;
    pms->abcd[1]  = 0xefcdab89;
    pms->abcd[2]  = 0x98badcfe;
    pms->abcd[3]  = 0x10325476;
}


//
// '_pdfioCryptoMD5Append()' - Append bytes to the MD5 hash.
//

void
_pdfioCryptoMD5Append(
    _pdfio_md5_t  *pms,			// I - MD5 state
    const uint8_t *data,		// I - Data to add
    size_t        nbytes)		// I - Number of bytes
{
  const uint8_t	*p = data;		// Pointer into data
  size_t	left = nbytes;		// Remaining bytes
  size_t	offset = (pms->count[0] >> 3) & 63;
					// Offset into state
  uint32_t	nbits = (uint32_t)(nbytes << 3);
					// Number of bits to add


  if (nbytes == 0)
    return;

  // Update the message length.
  pms->count[1] += (unsigned)(nbytes >> 29);
  pms->count[0] += nbits;
  if (pms->count[0] < nbits)
    pms->count[1] ++;

  // Process an initial partial block.
  if (offset)
  {
    size_t copy = ((offset + nbytes) > 64 ? 64 - offset : nbytes);
					// Number of bytes to copy

    memcpy(pms->buf + offset, p, copy);

    if ((offset + copy) < 64)
      return;

    p    += copy;
    left -= copy;

    md5_process(pms, pms->buf);
  }

  // Process full blocks.
  for (; left >= 64; p += 64, left -= 64)
    md5_process(pms, p);

  // Copy a final partial block.
  if (left)
    memcpy(pms->buf, p, left);
}


//
// '_pdfioCryptoMD5Finish()' - Finalize the MD5 hash.
//

void
_pdfioCryptoMD5Finish(
    _pdfio_md5_t *pms,			// I - MD5 state
    uint8_t      digest[16])		// O - Digest value
{
  int		i;			// Looping var
  uint8_t	data[8];		// Digest length data
  static const uint8_t pad[64] =	// Padding bytes
  {
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };


  // Save the length before padding.
  for (i = 0; i < 8; ++i)
    data[i] = (uint8_t)(pms->count[i >> 2] >> ((i & 3) << 3));

  // Pad to 56 bytes mod 64.
  _pdfioCryptoMD5Append(pms, pad, ((55 - (pms->count[0] >> 3)) & 63) + 1);

  // Append the length.
  _pdfioCryptoMD5Append(pms, data, 8);

  // Copy the digest from the state...
  for (i = 0; i < 16; ++i)
    digest[i] = (uint8_t)(pms->abcd[i >> 2] >> ((i & 3) << 3));
}
