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
// Local globals...
//

static uint8_t pdf_passpad[32] =	// Padding for passwords
{
  0x28, 0xbf, 0x4e, 0x5e, 0x4e, 0x75, 0x8a, 0x41,
  0x64, 0x00, 0x4e, 0x56, 0xff, 0xfa, 0x01, 0x08,
  0x2e, 0x2e, 0x00, 0xb6, 0xd0, 0x68, 0x3e, 0x80,
  0x2f, 0x0c, 0xa9, 0xfe, 0x64, 0x53, 0x69, 0x7a
};


//
// '_pdfioCryptoLock()' - Lock a PDF file by generating the encryption object and keys.
//

bool					// O - `true` on success, `false` otherwise
_pdfioCryptoLock(
    pdfio_file_t       *pdf,		// I - PDF file
    pdfio_permission_t permissions,	// I - Use permissions
    pdfio_encryption_t encryption,	// I - Type of encryption to use
    const char         *owner_password,	// I - Owner password, if any
    const char         *user_password)	// I - User password, if any
{
  pdfio_dict_t	*dict;			// Encryption dictionary
  size_t	i, j;			// Looping vars
  _pdfio_md5_t	md5;			// MD5 context
  uint8_t	digest[16];		// 128-bit MD5 digest
  _pdfio_rc4_t	rc4;			// RC4 encryption context
  size_t	len;			// Length of password
  uint8_t	owner_pad[32],		// Padded owner password
		user_pad[32],		// Padded user password
		perm_bytes[4],		// Permissions bytes
		*file_id;		// File ID bytes
  size_t	file_id_len;		// Length of file ID
  pdfio_dict_t	*cf_dict,		// CF dictionary
		*filter_dict;		// CryptFilter dictionary


  if ((dict = pdfioDictCreate(pdf)) == NULL)
  {
    _pdfioFileError(pdf, "Unable to create encryption dictionary.");
    return (false);
  }

  pdfioDictSetName(dict, "Filter", "Standard");

  switch (encryption)
  {
    case PDFIO_ENCRYPTION_RC4_128 :
    case PDFIO_ENCRYPTION_AES_128 :
        // Create the 128-bit encryption keys...
        if (user_password)
        {
          // Use the specified user password
          if ((len = strlen(user_password)) > sizeof(user_pad))
            len = sizeof(user_pad);
        }
        else
	{
	  // No user password
	  len = 0;
	}

	if (len > 0)
	  memcpy(user_pad, user_password, len);
	if (len < sizeof(user_pad))
	  memcpy(user_pad + len, pdf_passpad, sizeof(user_pad) - len);

        if (owner_password)
        {
          // Use the specified owner password...
          if ((len = strlen(owner_password)) > sizeof(owner_pad))
            len = sizeof(owner_pad);
	}
	else if (user_password && *user_password)
	{
	  // Generate a random owner password...
	  _pdfioCryptoMakeRandom(owner_pad, sizeof(owner_pad));
	  len = sizeof(owner_pad);
	}
	else
	{
	  // No owner password
	  len = 0;
	}

	if (len > 0)
	  memcpy(owner_pad, owner_password, len);
	if (len < sizeof(owner_pad))
	  memcpy(owner_pad + len, pdf_passpad, sizeof(owner_pad) - len);

        // Compute the owner key...
	_pdfioCryptoMD5Init(&md5);
	_pdfioCryptoMD5Append(&md5, owner_pad, 32);
	_pdfioCryptoMD5Finish(&md5, digest);

	for (i = 0; i < 50; i ++)
	{
	  _pdfioCryptoMD5Init(&md5);
	  _pdfioCryptoMD5Append(&md5, digest, 16);
	  _pdfioCryptoMD5Finish(&md5, digest);
	}

	// Copy and encrypt the padded user password...
	memcpy(pdf->owner_key, user_pad, sizeof(pdf->owner_key));

	for (i = 0; i < 20; i ++)
	{
	  uint8_t encrypt_key[16];	// RC4 encryption key

	  // XOR each byte in the digest with the loop counter to make a key...
	  for (j = 0; j < sizeof(encrypt_key); j ++)
	    encrypt_key[j] = (uint8_t)(digest[j] ^ i);

	  _pdfioCryptoRC4Init(&rc4, encrypt_key, sizeof(encrypt_key));
	  _pdfioCryptoRC4Crypt(&rc4, pdf->owner_key, pdf->owner_key, sizeof(pdf->owner_key));
	}
	pdf->owner_keylen = 32;

        // Generate the encryption key
	perm_bytes[0] = (uint8_t)permissions;
	perm_bytes[1] = (uint8_t)(permissions >> 8);
	perm_bytes[2] = (uint8_t)(permissions >> 16);
	perm_bytes[3] = (uint8_t)(permissions >> 24);

        file_id = pdfioArrayGetBinary(pdf->id_array, 0, &file_id_len);

	_pdfioCryptoMD5Init(&md5);
	_pdfioCryptoMD5Append(&md5, user_pad, 32);
	_pdfioCryptoMD5Append(&md5, pdf->owner_key, 32);
	_pdfioCryptoMD5Append(&md5, perm_bytes, 4);
	_pdfioCryptoMD5Append(&md5, file_id, file_id_len);
	_pdfioCryptoMD5Finish(&md5, digest);

	// MD5 the result 50 times..
	for (i = 0; i < 50; i ++)
	{
	  _pdfioCryptoMD5Init(&md5);
	  _pdfioCryptoMD5Append(&md5, digest, 16);
	  _pdfioCryptoMD5Finish(&md5, digest);
	}

	memcpy(pdf->encryption_key, digest, 16);
	pdf->encryption_keylen = 16;

	// Generate the user key...
        _pdfioCryptoMD5Init(&md5);
        _pdfioCryptoMD5Append(&md5, pdf_passpad, 32);
        _pdfioCryptoMD5Append(&md5, file_id, file_id_len);
        _pdfioCryptoMD5Finish(&md5, pdf->user_key);

        memset(pdf->user_key + 16, 0, 16);

        // Encrypt the result 20 times...
        for (i = 0; i < 20; i ++)
	{
	  // XOR each byte in the key with the loop counter...
	  for (j = 0; j < 16; j ++)
	    digest[j] = (uint8_t)(pdf->encryption_key[j] ^ i);

          _pdfioCryptoRC4Init(&rc4, digest, 16);
          _pdfioCryptoRC4Crypt(&rc4, pdf->user_key, pdf->user_key, sizeof(pdf->user_key));
	}
	pdf->user_keylen = 32;

	// Save everything in the dictionary...
	pdfioDictSetNumber(dict, "Length", 128);
	pdfioDictSetBinary(dict, "O", pdf->owner_key, sizeof(pdf->owner_key));
	pdfioDictSetNumber(dict, "P", (int)permissions);
	pdfioDictSetNumber(dict, "R", encryption == PDFIO_ENCRYPTION_RC4_128 ? 3 : 4);
	pdfioDictSetNumber(dict, "V", encryption == PDFIO_ENCRYPTION_RC4_128 ? 2 : 4);
	pdfioDictSetBinary(dict, "U", pdf->user_key, sizeof(pdf->user_key));
	
        if (encryption == PDFIO_ENCRYPTION_AES_128)
        {
	  if ((cf_dict = pdfioDictCreate(pdf)) == NULL)
	  {
	    _pdfioFileError(pdf, "Unable to create Encryption CF dictionary.");
	    return (false);
	  }

	  if ((filter_dict = pdfioDictCreate(pdf)) == NULL)
	  {
	    _pdfioFileError(pdf, "Unable to create Encryption CryptFilter dictionary.");
	    return (false);
	  }

	  pdfioDictSetName(filter_dict, "Type", "CryptFilter");
	  pdfioDictSetName(filter_dict, "CFM", encryption == PDFIO_ENCRYPTION_RC4_128 ? "V2" : "AESV2");
	  pdfioDictSetDict(cf_dict, "PDFio", filter_dict);
	  pdfioDictSetDict(dict, "CF", cf_dict);
	  pdfioDictSetName(dict, "StmF", "PDFio");
	  pdfioDictSetName(dict, "StrF", "PDFio");
	  pdfioDictSetBoolean(dict, "EncryptMetadata", true);
	}
	break;

    case PDFIO_ENCRYPTION_AES_256 :
        // TODO: Implement AES-256 (/V 6 /R 6)

    default :
        _pdfioFileError(pdf, "Encryption mode %d not supported for writing.", (int)encryption);
	return (false);
  }

  if ((pdf->encrypt_obj = pdfioFileCreateObj(pdf, dict)) == NULL)
  {
    _pdfioFileError(pdf, "Unable to create encryption object.");
    return (false);
  }

  pdfioObjClose(pdf->encrypt_obj);

  pdf->encryption  = encryption;
  pdf->permissions = permissions;

  return (true);
}


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
	if (pdf->encryption == PDFIO_ENCRYPTION_AES_128)
	  _pdfioCryptoMD5Append(&md5, (const uint8_t *)"sAlT", 4);
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
	if (pdf->encryption == PDFIO_ENCRYPTION_AES_128)
	  _pdfioCryptoMD5Append(&md5, (const uint8_t *)"sAlT", 4);
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


//
// '_pdfioCryptoUnlock()' - Unlock an encrypted PDF.
//

bool					// O - `true` on success, `false` otherwise
_pdfioCryptoUnlock(
    pdfio_file_t        *pdf,		// I - PDF file
    pdfio_password_cb_t password_cb,	// I - Password callback or `NULL` for none
    void                *password_data)	// I - Password callback data, if any
{
  int		tries;			// Number of tries
  const char	*password = NULL;	// Password to try
  pdfio_dict_t	*encrypt_dict;		// Encrypt objection dictionary
  int		version,		// Version value
		revision,		// Revision value
		length;			// Key length value
  const char	*handler,		// Security handler name
		*stream_filter,		// Stream encryption filter
		*string_filter;		// String encryption filter
  pdfio_dict_t	*cf_dict;		// CryptFilters dictionary
  unsigned char	*owner_key,		// Owner key
		*user_key,		// User key
		*file_id;		// File ID value
  size_t	owner_keylen,		// Length of owner key
		user_keylen,		// Length of user key
		file_idlen;		// Length of file ID


  // See if we support the type of encryption specified by the Encrypt object
  // dictionary...
  if ((encrypt_dict = pdfioObjGetDict(pdf->encrypt_obj)) == NULL)
  {
    _pdfioFileError(pdf, "Unable to get encryption dictionary.");
    return (false);
  }

  handler       = pdfioDictGetName(encrypt_dict, "Filter");
  version       = pdfioDictGetNumber(encrypt_dict, "V");
  revision      = pdfioDictGetNumber(encrypt_dict, "R");
  length        = pdfioDictGetNumber(encrypt_dict, "Length");
  stream_filter = pdfioDictGetName(encrypt_dict, "StmF");
  string_filter = pdfioDictGetName(encrypt_dict, "StrF");
  cf_dict       = pdfioDictGetDict(encrypt_dict, "CF");

  if (!handler || strcmp(handler, "Standard"))
  {
    _pdfioFileError(pdf, "Unsupported security handler '%s'.", handler ? handler : "(null)");
    return (false);
  }

  if (version == 4 && revision == 4)
  {
    // Lookup crypt filter to see if we support it...
    pdfio_dict_t *filter;		// Crypt Filter
    const char	*cfm;			// Crypt filter method

    if ((filter = pdfioDictGetDict(cf_dict, stream_filter)) != NULL && (cfm = pdfioDictGetName(filter, "CFM")) != NULL)
    {
      if (!strcmp(cfm, "V2"))
      {
        pdf->encryption = PDFIO_ENCRYPTION_RC4_128;
	if (length < 40 || length > 128)
	  length = 128;
      }
      if (!strcmp(cfm, "AESV2"))
      {
        pdf->encryption = PDFIO_ENCRYPTION_AES_128;
        length = 128;
      }
    }
  }
  else if (version == 2)
  {
    if (revision == 2)
    {
      pdf->encryption = PDFIO_ENCRYPTION_RC4_40;
      length = 40;
    }
    else if (revision == 3)
    {
      pdf->encryption = PDFIO_ENCRYPTION_RC4_128;
      if (length < 40 || length > 128)
        length = 128;
    }
  }
  // TODO: Implement AES-256 - V6 R6

  if (pdf->encryption == PDFIO_ENCRYPTION_NONE)
  {
    _pdfioFileError(pdf, "Unsupported encryption V%d R%d.", version, revision);
    return (false);
  }

  // Grab the remaining values we need to unlock the PDF...
  pdf->encryption_keylen = length / 8;
  pdf->permissions       = pdfioDictGetNumber(encrypt_dict, "P");

  owner_key = pdfioDictGetBinary(encrypt_dict, "O", &owner_keylen);
  user_key  = pdfioDictGetBinary(encrypt_dict, "U", &user_keylen);

  if (!owner_key || owner_keylen < 32 || owner_keylen > sizeof(pdf->owner_key))
  {
    _pdfioFileError(pdf, "Missing or bad owner key, unable to unlock file.");
    return (false);
  }

  memcpy(pdf->owner_key, owner_key, owner_keylen);
  pdf->owner_keylen = owner_keylen;

  if (!user_key || user_keylen < 32 || user_keylen > sizeof(pdf->user_key))
  {
    _pdfioFileError(pdf, "Missing or bad user key, unable to unlock file.");
    return (false);
  }

  memcpy(pdf->user_key, user_key, user_keylen);
  pdf->user_keylen = user_keylen;

  if ((file_id = pdfioArrayGetBinary(pdf->id_array, 0, &file_idlen)) == NULL || file_idlen < 16)
  {
    _pdfioFileError(pdf, "Missing or bad file ID, unable to unlock file.");
    return (false);
  }
  
  // Now try to unlock the PDF...
  for (tries = 0; tries < 4; tries ++)
  {
    // 
  }

  return (false);
}
