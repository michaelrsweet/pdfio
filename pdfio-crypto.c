//
// Cryptographic support functions for PDFio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

//
// Include necessary headers...
//

#include "pdfio-private.h"
#if !_WIN32
#  include <sys/time.h>
#endif // !_WIN32
#ifdef __has_include
#  if __has_include(<sys/random.h>)
#    define HAVE_GETRANDOM 1
#    include <sys/random.h>
#  endif // __has_include(<sys/random.h>)
#endif // __has_include


//
// '_pdfioCryptoMakeRandom()' - Fill a buffer with good random numbers.
//

void
_pdfioCryptoMakeRandom(uint8_t *buffer,	// I - Buffer
                       size_t  bytes)	// I - Number of bytes
{
#ifdef __APPLE__
  // macOS/iOS provide the arc4random function which is seeded with entropy
  // from the system...
  while (bytes > 0)
  {
    // Just collect 8 bits from each call to fill the buffer...
    *buffer++ = (uint8_t)arc4random();
    bytes --;
  }

#else
#  if _WIN32
  // Windows provides the CryptGenRandom function...
  HCRYPTPROV	prov;			// Cryptographic provider

  if (CryptAcquireContextA(&prov, NULL, NULL, PROV_RSA_FULL, 0))
  {
    // Got the default crypto provider, try to get random data...
    BOOL success = CryptGenRandom(prov, (DWORD)bytes, buffer);

    // Release the crypto provider and return on success...
    CryptReleaseContext(prov, 0);

    if (success)
      return;
  }

#  elif HAVE_GETRANDOM
  // Linux provides a system call called getrandom that uses system entropy ...
  ssize_t	rbytes;			// Bytes read

  while (bytes > 0)
  {
    if ((rbytes = getrandom(buffer, bytes, 0)) < 0)
    {
      if (errno != EINTR && errno != EAGAIN)
	break;
    }
    bytes -= (size_t)rbytes;
    buffer += rbytes;
  }

  if (bytes == 0)
    return;

#  else
  // Other UNIX-y systems have /dev/urandom...
  int		fd;			// Random number file
  ssize_t	rbytes;			// Bytes read


  // Fall back on /dev/urandom...
  if ((fd = open("/dev/urandom", O_RDONLY)) >= 0)
  {
    while (bytes > 0)
    {
      if ((rbytes = read(fd, buffer, bytes)) < 0)
      {
        if (errno != EINTR && errno != EAGAIN)
          break;
      }
      bytes -= (size_t)rbytes;
      buffer += rbytes;
    }

    close(fd);

    if (bytes == 0)
      return;
  }
#  endif // _WIN32

  // If we get here then we were unable to get enough random data or the local
  // system doesn't have enough entropy.  Make some up...
  uint32_t	i,			// Looping var
		mt_state[624],		// Mersenne twister state
		mt_index,		// Mersenne twister index
		temp;			// Temporary value
#  if _WIN32
  struct _timeb curtime;		// Current time

  _ftime(&curtime);
  mt_state[0] = (uint32_t)(curtime.time + curtime.millitm);

#  else
  struct timeval curtime;		// Current time

  gettimeofday(&curtime, NULL);
  mt_state[0] = (uint32_t)(curtime.tv_sec + curtime.tv_usec);
#  endif // _WIN32

  // Seed the random number state...
  mt_index = 0;

  for (i = 1; i < 624; i ++)
    mt_state[i] = (uint32_t)((1812433253 * (mt_state[i - 1] ^ (mt_state[i - 1] >> 30))) + i);

  // Fill the buffer with random numbers...
  while (bytes > 0)
  {
    if (mt_index == 0)
    {
      // Generate a sequence of random numbers...
      uint32_t i1 = 1, i397 = 397;	// Looping vars

      for (i = 0; i < 624; i ++)
      {
	temp        = (mt_state[i] & 0x80000000) + (mt_state[i1] & 0x7fffffff);
	mt_state[i] = mt_state[i397] ^ (temp >> 1);

	if (temp & 1)
	  mt_state[i] ^= 2567483615u;

	i1 ++;
	i397 ++;

	if (i1 == 624)
	  i1 = 0;

	if (i397 == 624)
	  i397 = 0;
      }
    }

    // Pull 32-bits of random data...
    temp = mt_state[mt_index ++];
    temp ^= temp >> 11;
    temp ^= (temp << 7) & 2636928640u;
    temp ^= (temp << 15) & 4022730752u;
    temp ^= temp >> 18;

    if (mt_index == 624)
      mt_index = 0;

    // Copy to the buffer...
    switch (bytes)
    {
      case 1 :
          *buffer++ = (uint8_t)(temp >> 24);
          bytes --;
          break;
      case 2 :
          *buffer++ = (uint8_t)(temp >> 24);
          *buffer++ = (uint8_t)(temp >> 16);
          bytes -= 2;
          break;
      case 3 :
          *buffer++ = (uint8_t)(temp >> 24);
          *buffer++ = (uint8_t)(temp >> 16);
          *buffer++ = (uint8_t)(temp >> 8);
          bytes -= 3;
          break;
      default :
          *buffer++ = (uint8_t)(temp >> 24);
          *buffer++ = (uint8_t)(temp >> 16);
          *buffer++ = (uint8_t)(temp >> 8);
          *buffer++ = (uint8_t)temp;
          bytes -= 4;
          break;
    }
  }
#endif // __APPLE__
}


//
// '_pdfioCryptoMakeReader()' - Setup a cryptographic context and callback for reading.
//

_pdfio_crypto_cb_t			// O  - Decryption callback or `NULL` for none
 _pdfioCryptoMakeReader(
     pdfio_file_t        *pdf,		// I  - PDF file
     pdfio_obj_t         *obj,		// I  - PDF object
     _pdfio_crypto_ctx_t *ctx,		// I  - Pointer to crypto context
     uint8_t             *iv,		// I  - Buffer for initialization vector
     size_t              *ivlen)	// IO - Size of initialization vector
{
  uint8_t	data[21];		/* Key data */
  _pdfio_md5_t	md5;			/* MD5 state */
  uint8_t	digest[16];		/* MD5 digest value */


  // Range check input...
  if (!pdf)
  {
    *ivlen = 0;
    return (NULL);
  }

  switch (pdf->encryption)
  {
    default :
        *ivlen = 0;
        return (NULL);

    case PDFIO_ENCRYPTION_RC4_128 :
    case PDFIO_ENCRYPTION_AES_128 :
	// Copy the key data for the MD5 hash.
	memcpy(data, pdf->encryption_key, sizeof(pdf->encryption_key));
	data[16] = (uint8_t)obj->number;
	data[17] = (uint8_t)(obj->number >> 8);
	data[18] = (uint8_t)(obj->number >> 16);
	data[19] = (uint8_t)obj->generation;
	data[20] = (uint8_t)(obj->generation >> 8);

        // Hash it...
        _pdfioCryptoMD5Init(&md5);
	_pdfioCryptoMD5Append(&md5, data, sizeof(data));
	_pdfioCryptoMD5Finish(&md5, digest);

        // Initialize the RC4/AES context using the digest...
        if (pdf->encryption == PDFIO_ENCRYPTION_RC4_128)
        {
	  *ivlen = 0;
          _pdfioCryptoRC4Init(&ctx->rc4, digest, sizeof(digest));
          return ((_pdfio_crypto_cb_t)_pdfioCryptoRC4Crypt);
	}
	else
	{
	  *ivlen = 16;
          _pdfioCryptoAESInit(&ctx->aes, digest, sizeof(digest), iv);
          return ((_pdfio_crypto_cb_t)_pdfioCryptoAESDecrypt);
	}
  }
}


//
// '_pdfioCryptoMakeWriter()' - Setup a cryptographic context and callback for writing.
//

_pdfio_crypto_cb_t			// O  - Encryption callback or `NULL` for none
 _pdfioCryptoMakeWriter(
     pdfio_file_t        *pdf,		// I  - PDF file
     pdfio_obj_t         *obj,		// I  - PDF object
     _pdfio_crypto_ctx_t *ctx,		// I  - Pointer to crypto context
     uint8_t             *iv,		// I  - Buffer for initialization vector
     size_t              *ivlen)	// IO - Size of initialization vector
{
  uint8_t	data[21];		/* Key data */
  _pdfio_md5_t	md5;			/* MD5 state */
  uint8_t	digest[16];		/* MD5 digest value */


  // Range check input...
  if (!pdf)
  {
    *ivlen = 0;
    return (NULL);
  }

  switch (pdf->encryption)
  {
    default :
        *ivlen = 0;
        return (NULL);

    case PDFIO_ENCRYPTION_RC4_128 :
    case PDFIO_ENCRYPTION_AES_128 :
	// Copy the key data for the MD5 hash.
	memcpy(data, pdf->encryption_key, sizeof(pdf->encryption_key));
	data[16] = (uint8_t)obj->number;
	data[17] = (uint8_t)(obj->number >> 8);
	data[18] = (uint8_t)(obj->number >> 16);
	data[19] = (uint8_t)obj->generation;
	data[20] = (uint8_t)(obj->generation >> 8);

        // Hash it...
        _pdfioCryptoMD5Init(&md5);
	_pdfioCryptoMD5Append(&md5, data, sizeof(data));
	_pdfioCryptoMD5Finish(&md5, digest);

        // Initialize the RC4/AES context using the digest...
        if (pdf->encryption == PDFIO_ENCRYPTION_RC4_128)
        {
	  *ivlen = 0;
          _pdfioCryptoRC4Init(&ctx->rc4, digest, sizeof(digest));
          return ((_pdfio_crypto_cb_t)_pdfioCryptoRC4Crypt);
	}
	else
	{
	  *ivlen = 16;
	  _pdfioCryptoMakeRandom(iv, *ivlen);
          _pdfioCryptoAESInit(&ctx->aes, digest, sizeof(digest), iv);
          return ((_pdfio_crypto_cb_t)_pdfioCryptoAESEncrypt);
	}
  }
}
