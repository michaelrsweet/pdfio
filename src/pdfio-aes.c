//
// AES functions for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//
// AES code is adapted from the "tiny-AES-c" project
// (<https://github.com/kokke/tiny-AES-c>)
//

//
// Include necessary headers...
//

#include "pdfio-private.h"


//
// Local types...
//

typedef uint8_t state_t[4][4];		// 4x4 AES state table


//
// Local globals...
//

static const uint8_t sbox[256] =	// S-box lookup table
{
  //0     1    2      3     4    5     6     7      8    9     A      B    C     D     E     F
  0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
  0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
  0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
  0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
  0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
  0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
  0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
  0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
  0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
  0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
  0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
  0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
  0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
  0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
  0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
  0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
};
static const uint8_t rsbox[256] =	// Reverse S-box lookup table
{
  0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
  0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
  0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
  0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
  0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
  0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
  0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
  0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
  0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
  0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
  0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
  0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
  0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
  0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
  0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
  0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
};

// The round constant word array, Rcon[i], contains the values given by
// x to the power (i-1) being powers of x (x is denoted as {02}) in the field GF(2^8)
static const uint8_t Rcon[11] =		// Round constants
{
  0x8d, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36
};


//
// Local functions...
//

static void	AddRoundKey(size_t round, state_t *state, const uint8_t *RoundKey);
static void	SubBytes(state_t *state);
static void	ShiftRows(state_t *state);
static uint8_t	xtime(uint8_t x);
static void	MixColumns(state_t *state);
static uint8_t	Multiply(uint8_t x, uint8_t y);
static void	InvMixColumns(state_t *state);
static void	InvSubBytes(state_t *state);
static void	InvShiftRows(state_t *state);
static void	Cipher(state_t *state, const _pdfio_aes_t *ctx);
static void	InvCipher(state_t *state, const _pdfio_aes_t *ctx);
static void	XorWithIv(uint8_t *buf, const uint8_t *Iv);


//
// '_pdfioCryptoAESInit()' - Initialize an AES context.
//

void
_pdfioCryptoAESInit(
    _pdfio_aes_t  *ctx,			// I - AES context
    const uint8_t *key,			// I - Key
    size_t        keylen,		// I - Length of key (must be 16 or 32)
    const uint8_t *iv)			// I - 16-byte initialization vector
{
  size_t	i;			// Looping var
  uint8_t	*rkptr0,		// Previous round_key values
		*rkptr,			// Current round_key values
		*rkend,			// End of round_key values
		tempa[4];		// Used for the column/row operations
//  size_t	roundlen = keylen + 24;	// Length of round_key
  size_t	nwords = keylen / 4;	// Number of 32-bit words in key


  // Clear context
  memset(ctx, 0, sizeof(_pdfio_aes_t));
  ctx->round_size = keylen / 4 + 6;

  // The first round key is the key itself.
  memcpy(ctx->round_key, key, keylen);

  // All other round keys are found from the previous round keys.
  for (rkptr0 = ctx->round_key, rkptr = rkptr0 + keylen, rkend = rkptr + 16 * ctx->round_size, i = nwords; rkptr < rkend; i ++)
  {
    if ((i % nwords) == 0)
    {
      // Shifts word left once - [a0,a1,a2,a3] becomes [a1,a2,a3,a0], then
      // apply the S-box to each of the four bytes to produce an output word.
      tempa[0] = sbox[rkptr[-3]] ^ Rcon[i / nwords];
      tempa[1] = sbox[rkptr[-2]];
      tempa[2] = sbox[rkptr[-1]];
      tempa[3] = sbox[rkptr[-4]];
    }
    else if (keylen == 32 && (i % nwords) == 4)
    {
      // Apply the S-box to each of the four bytes to produce an output word.
      tempa[0] = sbox[rkptr[-4]];
      tempa[1] = sbox[rkptr[-3]];
      tempa[2] = sbox[rkptr[-2]];
      tempa[3] = sbox[rkptr[-1]];
    }
    else
    {
      // Use unshifted values without S-box...
      tempa[0] = rkptr[-4];
      tempa[1] = rkptr[-3];
      tempa[2] = rkptr[-2];
      tempa[3] = rkptr[-1];
    }

    // TODO: Optimize to incorporate this into previous steps
    *rkptr++ = *rkptr0++ ^ tempa[0];
    *rkptr++ = *rkptr0++ ^ tempa[1];
    *rkptr++ = *rkptr0++ ^ tempa[2];
    *rkptr++ = *rkptr0++ ^ tempa[3];
  }

  // Copy the initialization vector...
  if (iv)
    memcpy(ctx->iv, iv, sizeof(ctx->iv));
}


//
// '_pdfioCryptoAESDecrypt()' - Decrypt a block of bytes with AES.
//
// "inbuffer" and "outbuffer" can point to the same memory.  Length must be a
// multiple of 16 bytes (excess is not decrypted).
//

size_t					// O - Number of bytes in output buffer
_pdfioCryptoAESDecrypt(
    _pdfio_aes_t  *ctx,			// I - AES context
    uint8_t       *outbuffer,		// I - Output buffer
    const uint8_t *inbuffer,		// I - Input buffer
    size_t        len)			// I - Number of bytes to decrypt
{
  uint8_t	next_iv[16];		// Next IV value
  size_t	outbytes = 0;		// Output bytes


  if (inbuffer != outbuffer)
  {
    // Not the most efficient, but we can optimize later - the sample AES code
    // manipulates the data directly in memory and doesn't support separate
    // input and output buffers...
    memcpy(outbuffer, inbuffer, len);
  }

  while (len > 15)
  {
    memcpy(next_iv, outbuffer, 16);
    InvCipher((state_t *)outbuffer, ctx);
    XorWithIv(outbuffer, ctx->iv);
    memcpy(ctx->iv, next_iv, 16);
    outbuffer += 16;
    len -= 16;
    outbytes += 16;
  }

  return (outbytes);
}


//
// '_pdfioCryptoAESEncrypt()' - Encrypt a block of bytes with AES.
//
// "inbuffer" and "outbuffer" can point to the same memory.  "outbuffer" must
// be a multiple of 16 bytes.
//

size_t					// O - Number of bytes in output buffer
_pdfioCryptoAESEncrypt(
    _pdfio_aes_t  *ctx,			// I - AES context
    uint8_t       *outbuffer,		// I - Output buffer
    const uint8_t *inbuffer,		// I - Input buffer
    size_t        len)			// I - Number of bytes to decrypt
{
  uint8_t	*iv = ctx->iv;		// Current IV for CBC
  size_t	outbytes = 0;		// Output bytes


  if (len == 0)
    return (0);

  if (inbuffer != outbuffer)
  {
    // Not the most efficient, but we can optimize later - the sample AES code
    // manipulates the data directly in memory and doesn't support separate
    // input and output buffers...
    memcpy(outbuffer, inbuffer, len);
  }

  while (len > 15)
  {
    XorWithIv(outbuffer, iv);
    Cipher((state_t*)outbuffer, ctx);
    iv = outbuffer;
    outbuffer += 16;
    len -= 16;
    outbytes += 16;
  }

  if (len > 0)
  {
    // Pad the final buffer with (16 - len)...
    memset(outbuffer + len, 16 - len, 16 - len);

    XorWithIv(outbuffer, iv);
    Cipher((state_t*)outbuffer, ctx);
    iv = outbuffer;
    outbytes += 16;
  }

  /* store Iv in ctx for next call */
  memcpy(ctx->iv, iv, 16);

  return (outbytes);
}


// This function adds the round key to state.
// The round key is added to the state by an XOR function.
static void
AddRoundKey(size_t round, state_t *state, const uint8_t *RoundKey)
{
  unsigned	i;			// Looping var
  uint8_t	*sptr = (*state)[0];	// Pointer into state


  for (RoundKey += round * 16, i = 16; i > 0; i --, sptr ++, RoundKey ++)
    *sptr ^= *RoundKey;
}


// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
static void
SubBytes(state_t *state)
{
  unsigned	i;			// Looping var
  uint8_t	*sptr = (*state)[0];	// Pointer into state


  for (i = 16; i > 0; i --, sptr ++)
    *sptr = sbox[*sptr];
}

// The ShiftRows() function shifts the rows in the state to the left.
// Each row is shifted with different offset.
// Offset = Row number. So the first row is not shifted.
static void
ShiftRows(state_t *state)
{
  uint8_t	*sptr = (*state)[0];	// Pointer into state
  uint8_t	temp;			// Temporary value


  // Rotate first row 1 columns to left
  temp     = sptr[1];
  sptr[1]  = sptr[5];
  sptr[5]  = sptr[9];
  sptr[9]  = sptr[13];
  sptr[13] = temp;

  // Rotate second row 2 columns to left
  temp     = sptr[2];
  sptr[2]  = sptr[10];
  sptr[10] = temp;

  temp     = sptr[6];
  sptr[6]  = sptr[14];
  sptr[14] = temp;

  // Rotate third row 3 columns to left
  temp     = sptr[3];
  sptr[3]  = sptr[15];
  sptr[15] = sptr[11];
  sptr[11] = sptr[7];
  sptr[7]  = temp;
}


static uint8_t
xtime(uint8_t x)
{
  return ((uint8_t)((x << 1) ^ ((x >> 7) * 0x1b)));
}


// MixColumns function mixes the columns of the state matrix
static void
MixColumns(state_t *state)
{
  unsigned	i;			// Looping var
  uint8_t	*sptr = (*state)[0];	// Pointer into state
  uint8_t	Tmp, Tm, t;		// Temporary values

  for (i = 4; i > 0; i --, sptr += 4)
  {
    t       = sptr[0];
    Tmp     = sptr[0] ^ sptr[1] ^ sptr[2] ^ sptr[3];
    Tm      = sptr[0] ^ sptr[1];
    Tm      = xtime(Tm);
    sptr[0] ^= Tm ^ Tmp;

    Tm      = sptr[1] ^ sptr[2];
    Tm      = xtime(Tm);
    sptr[1] ^= Tm ^ Tmp;

    Tm      = sptr[2] ^ sptr[3];
    Tm      = xtime(Tm);
    sptr[2] ^= Tm ^ Tmp;

    Tm      = sptr[3] ^ t;
    Tm      = xtime(Tm);
    sptr[3] ^= Tm ^ Tmp;
  }
}


// Multiply is used to multiply numbers in the field GF(2^8)
// Note: The last call to xtime() is unneeded, but often ends up generating a smaller binary
//       The compiler seems to be able to vectorize the operation better this way.
//       See https://github.com/kokke/tiny-AES-c/pull/34
static uint8_t Multiply(uint8_t x, uint8_t y)
{
  return (((y & 1) * x) ^
	  ((y>>1 & 1) * xtime(x)) ^
	  ((y>>2 & 1) * xtime(xtime(x))) ^
	  ((y>>3 & 1) * xtime(xtime(xtime(x)))) ^
	  ((y>>4 & 1) * xtime(xtime(xtime(xtime(x)))))); /* this last call to xtime() can be omitted */
}


// MixColumns function mixes the columns of the state matrix.
// The method used to multiply may be difficult to understand for the inexperienced.
// Please use the references to gain more information.
static void
InvMixColumns(state_t *state)
{
  unsigned	i;			// Looping var
  uint8_t	*sptr = (*state)[0];	// Pointer into state
  uint8_t	a, b, c, d;		// Temporary values


  for (i = 4; i > 0; i --)
  {
    a = sptr[0];
    b = sptr[1];
    c = sptr[2];
    d = sptr[3];

    *sptr++ = Multiply(a, 0x0e) ^ Multiply(b, 0x0b) ^ Multiply(c, 0x0d) ^ Multiply(d, 0x09);
    *sptr++ = Multiply(a, 0x09) ^ Multiply(b, 0x0e) ^ Multiply(c, 0x0b) ^ Multiply(d, 0x0d);
    *sptr++ = Multiply(a, 0x0d) ^ Multiply(b, 0x09) ^ Multiply(c, 0x0e) ^ Multiply(d, 0x0b);
    *sptr++ = Multiply(a, 0x0b) ^ Multiply(b, 0x0d) ^ Multiply(c, 0x09) ^ Multiply(d, 0x0e);
  }
}


// The SubBytes Function Substitutes the values in the
// state matrix with values in an S-box.
static void
InvSubBytes(state_t *state)
{
  unsigned	i;			// Looping var
  uint8_t	*sptr = (*state)[0];	// Pointer into state


  for (i = 16; i > 0; i --, sptr ++)
    *sptr = rsbox[*sptr];
}


static void
InvShiftRows(state_t *state)
{
  uint8_t	*sptr = (*state)[0];	// Pointer into state
  uint8_t	temp;			// Temporary value


  // Rotate first row 1 columns to right
  temp     = sptr[13];
  sptr[13] = sptr[9];
  sptr[9]  = sptr[5];
  sptr[5]  = sptr[1];
  sptr[1]  = temp;

  // Rotate second row 2 columns to right
  temp     = sptr[2];
  sptr[2]  = sptr[10];
  sptr[10] = temp;

  temp     = sptr[6];
  sptr[6]  = sptr[14];
  sptr[14] = temp;

  // Rotate third row 3 columns to right
  temp     = sptr[3];
  sptr[3]  = sptr[7];
  sptr[7]  = sptr[11];
  sptr[11] = sptr[15];
  sptr[15] = temp;
}


// Cipher is the main function that encrypts the PlainText.
static void
Cipher(state_t *state, const _pdfio_aes_t *ctx)
{
  size_t round = 0;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(0, state, ctx->round_key);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without MixColumns()
  for (round = 1; round < ctx->round_size; round ++)
  {
    SubBytes(state);
    ShiftRows(state);
    MixColumns(state);
    AddRoundKey(round, state, ctx->round_key);
  }
  // Add round key to last round
  SubBytes(state);
  ShiftRows(state);
  AddRoundKey(ctx->round_size, state, ctx->round_key);
}


static void
InvCipher(state_t *state, const _pdfio_aes_t *ctx)
{
  size_t round;

  // Add the First round key to the state before starting the rounds.
  AddRoundKey(ctx->round_size, state, ctx->round_key);

  // There will be Nr rounds.
  // The first Nr-1 rounds are identical.
  // These Nr rounds are executed in the loop below.
  // Last one without InvMixColumn()
  for (round = ctx->round_size - 1; ; round --)
  {
    InvShiftRows(state);
    InvSubBytes(state);
    AddRoundKey(round, state, ctx->round_key);

    if (round == 0)
      break;

    InvMixColumns(state);
  }
}


static void
XorWithIv(uint8_t *buf, const uint8_t *Iv)
{
  // 16-byte block...
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
  *buf++ ^= *Iv++;
}
