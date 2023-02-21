//
// RC4 functions for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Original code by Tim Martin
// Copyright © 1999 by Carnegie Mellon University, All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of Carnegie Mellon
// University not be used in advertising or publicity pertaining to
// distribution of the software without specific, written prior
// permission.
//
// CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
// THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
// FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE FOR
// ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
// OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include "pdfio-private.h"


//
// '_pdfioCryptoRC4Init()' - Initialize an RC4 context with the specified key.
//

void
_pdfioCryptoRC4Init(
    _pdfio_rc4_t  *ctx,			// IO - Context
    const uint8_t *key,			// I - Key
    size_t        keylen)		// I - Length of key
{
  size_t	i;			// Looping var
  uint8_t	j,			// S box counter
		tmp;			// Temporary variable


  // Fill in linearly s0=0, s1=1, ...
  for (i = 0; i < 256; i ++)
    ctx->sbox[i] = (uint8_t)i;

  for (i = 0, j = 0; i < 256; i ++)
  {
    // j = (j + Si + Ki) mod 256
    j += ctx->sbox[i] + key[i % keylen];

    // Swap Si and Sj...
    tmp          = ctx->sbox[i];
    ctx->sbox[i] = ctx->sbox[j];
    ctx->sbox[j] = tmp;
  }

  // Initialize counters to 0 and return...
  ctx->i = 0;
  ctx->j = 0;
}


//
// '_pdfioCryptoRC4Crypt()' - De/encrypt the given buffer.
//
// "inbuffer" and "outbuffer" can point to the same memory.
//

size_t					// O - Number of output bytes
_pdfioCryptoRC4Crypt(
    _pdfio_rc4_t  *ctx,			// I - Context
    uint8_t       *outbuffer,		// I - Output buffer
    const uint8_t *inbuffer,		// I - Input buffer
    size_t        len)			// I - Size of buffers
{
  uint8_t	tmp,			// Swap variable
		i, j,			// Looping vars
		t;			// Current S box
  size_t	outbytes = len;		// Number of output bytes


  // Loop through the entire buffer...
  i = ctx->i;
  j = ctx->j;

  while (len > 0)
  {
    // Get the next S box indices...
    i ++;
    j += ctx->sbox[i];

    // Swap Si and Sj...
    tmp          = ctx->sbox[i];
    ctx->sbox[i] = ctx->sbox[j];
    ctx->sbox[j] = tmp;

    // Get the S box index for this byte...
    t = ctx->sbox[i] + ctx->sbox[j];

    // Encrypt using the S box...
    *outbuffer++ = *inbuffer++ ^ ctx->sbox[t];
    len --;
  }

  // Copy current S box indices back to context...
  ctx->i = i;
  ctx->j = j;

  return (outbytes);
}
