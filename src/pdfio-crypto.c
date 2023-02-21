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
#if _WIN32
#  include <windows.h>
#  include <bcrypt.h>
#  include <sys/types.h>
#  include <sys/timeb.h>
#else
#  include <sys/time.h>
#endif // _WIN32
#ifdef __has_include
#  if __has_include(<sys/random.h>)
#    define HAVE_GETRANDOM 1
#    include <sys/random.h>
#  endif // __has_include(<sys/random.h>)
#endif // __has_include


//
// PDF files can use one of several methods to encrypt a PDF file.  There is
// an owner password that controls/unlocks full editing/usage permissions and a
// user password that unlocks limited usage of the PDF.  Permissions are set
// using bits for copy, print, etc. (see the `pdfio_permission_t` enumeration).
// Passwords can be up to 32 bytes in length, with a well-known padding string
// that is applied if the string is less than 32 bytes or there is no password.
//
// > Note: PDF encryption has several design weaknesses which limit the
// > protection offered.  The V2 and V4 security handlers depend on the obsolete
// > MD5 and RC4 algorithms for key generation, and Cipher Block Chaining (CBC)
// > further weakens AES support.  Enforcement of usage permissions depends on
// > the consuming software honoring them, so if the password is known or (more
// > commonly) the user password is blank, it is possible to bypass usage
// > permissions completely.
//
// PDFio supports the following:
//
// - The original 40-bit RC4 (V2+R2) encryption for reading only
// - 128-bit RC4 (V2+R3) encryption for reading and writing
// - 128-bit AES (V4+R4) encryption for reading and writing
// - TODO: 256-bit AES (V6+R6) encryption for reading and writing
//
// Common values:
//
// - "F" is the file encryption key (40 to 256 bits/5 to 32 bytes)
// - "Fid" is the file ID string (stored in PDF file, 32 bytes)
// - "O" is the owner key (stored in PDF file, 32 bytes)
// - "Opad" is the padded owner password (32 bytes)
// - "P" is the permissions integer (stored in PDF file)
// - "P4" is the permissions integer as a 32-bit little-endian value
// - "U" is the user key (stored in PDF file, 32 bytes)
// - "Upad" is the padded user password (32 bytes)
//
// V2+R2 handler:
//
//   F = md5(Upad+O+P4+Fid)
//   O = rc4(Upad, md5(Opad))
//       (unlock with owner password)
//       Upad = rc4(O, md5(Opad))
//   U = rc4(md5(Upad+Fid)+0[16], F)
//
// V2+R3/V4+R4 handler:
//
//   F = md5(md5(Upad+O+P4+Fid))^50
//   O = rc4(Upad, md5(md5(Opad))^50)^20
//       (unlock with owner password)
//       Upad = rc4(O, md5(md5(Opad))^50)^20
//   U = rc4(md5(Upad+Fid)+0[16], F)^20
//
// V6+R6 handler:
//
//   TODO: document V6+R6 handler
//

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
// Local functions...
//

static void	decrypt_user_key(pdfio_encryption_t encryption, const uint8_t *file_key, uint8_t user_key[32]);
static void	encrypt_user_key(pdfio_encryption_t encryption, const uint8_t *file_key, uint8_t user_key[32]);
static void	make_file_key(pdfio_encryption_t encryption, pdfio_permission_t permissions, const unsigned char *file_id, size_t file_idlen, const uint8_t *user_pad, const uint8_t *owner_key, uint8_t file_key[16]);
static void	make_owner_key(pdfio_encryption_t encryption, const uint8_t *owner_pad, const uint8_t *user_pad, uint8_t owner_key[32]);
static void	make_user_key(const unsigned char *file_id, size_t file_idlen, uint8_t user_key[32]);
static void	pad_password(const char *password, uint8_t pad[32]);


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
  uint8_t	owner_pad[32],		// Padded owner password
		user_pad[32],		// Padded user password
		*file_id;		// File ID bytes
  size_t	file_idlen;		// Length of file ID
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
        pad_password(user_password, user_pad);

        if (!owner_password && user_password && *user_password)
        {
	  // Generate a random owner password...
	  _pdfioCryptoMakeRandom(owner_pad, sizeof(owner_pad));
	}
	else
	{
	  // Use supplied owner password
	  pad_password(owner_password, owner_pad);
	}

        // Compute the owner key...
        make_owner_key(encryption, owner_pad, user_pad, pdf->owner_key);
	pdf->owner_keylen = 32;

        // Generate the encryption key
        file_id = pdfioArrayGetBinary(pdf->id_array, 0, &file_idlen);

        make_file_key(encryption, permissions, file_id, file_idlen, user_pad, pdf->owner_key, pdf->file_key);
	pdf->file_keylen = 16;

	// Generate the user key...
	make_user_key(file_id, file_idlen, pdf->user_key);
	encrypt_user_key(encryption, pdf->file_key, pdf->user_key);
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
	  pdfioDictSetName(filter_dict, "CFM", "AESV2");
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


  PDFIO_DEBUG("_pdfioCryptoMakeReader(pdf=%p, obj=%p(%d), ctx=%p, iv=%p, ivlen=%p(%d))\n", pdf, obj, (int)obj->number, ctx, iv, ivlen, (int)*ivlen);

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

    case PDFIO_ENCRYPTION_RC4_40 :
	// Copy the key data for the MD5 hash.
	memcpy(data, pdf->file_key, sizeof(pdf->file_key));
	data[16] = (uint8_t)obj->number;
	data[17] = (uint8_t)(obj->number >> 8);
	data[18] = (uint8_t)(obj->number >> 16);
	data[19] = (uint8_t)obj->generation;
	data[20] = (uint8_t)(obj->generation >> 8);

        // Hash it...
        _pdfioCryptoMD5Init(&md5);
	_pdfioCryptoMD5Append(&md5, data, sizeof(data));
	_pdfioCryptoMD5Finish(&md5, digest);

        // Initialize the RC4 context using 40 bits of the digest...
	_pdfioCryptoRC4Init(&ctx->rc4, digest, 5);
	return ((_pdfio_crypto_cb_t)_pdfioCryptoRC4Crypt);

    case PDFIO_ENCRYPTION_RC4_128 :
    case PDFIO_ENCRYPTION_AES_128 :
	// Copy the key data for the MD5 hash.
	memcpy(data, pdf->file_key, sizeof(pdf->file_key));
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


  PDFIO_DEBUG("_pdfioCryptoMakeWriter(pdf=%p, obj=%p(%d), ctx=%p, iv=%p, ivlen=%p(%d))\n", pdf, obj, (int)obj->number, ctx, iv, ivlen, (int)*ivlen);

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
	memcpy(data, pdf->file_key, sizeof(pdf->file_key));
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
  _pdfio_md5_t	md5;			// MD5 context
  uint8_t	file_digest[16];	// MD5 digest of file ID and pad


  // See if we support the type of encryption specified by the Encrypt object
  // dictionary...
  if ((encrypt_dict = pdfioObjGetDict(pdf->encrypt_obj)) == NULL)
  {
    _pdfioFileError(pdf, "Unable to get encryption dictionary.");
    return (false);
  }

  handler  = pdfioDictGetName(encrypt_dict, "Filter");
  version  = (int)pdfioDictGetNumber(encrypt_dict, "V");
  revision = (int)pdfioDictGetNumber(encrypt_dict, "R");
  length   = (int)pdfioDictGetNumber(encrypt_dict, "Length");

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

    stream_filter = pdfioDictGetName(encrypt_dict, "StmF");
    string_filter = pdfioDictGetName(encrypt_dict, "StrF");
    cf_dict       = pdfioDictGetDict(encrypt_dict, "CF");

    if (!cf_dict)
    {
      _pdfioFileError(pdf, "Missing encryption filter dictionary.");
      return (false);
    }
    else if (!stream_filter)
    {
      _pdfioFileError(pdf, "Missing stream encryption filter.");
      return (false);
    }
    else if (!string_filter)
    {
      _pdfioFileError(pdf, "Missing string encryption filter.");
      return (false);
    }
    else if (strcmp(stream_filter, string_filter))
    {
      _pdfioFileError(pdf, "Different stream and string encryption filters - not supported.");
      return (false);
    }
    else if ((filter = pdfioDictGetDict(cf_dict, stream_filter)) == NULL)
    {
      _pdfioFileError(pdf, "Missing stream encryption filter '%s'.", stream_filter);
      return (false);
    }
    else if ((cfm = pdfioDictGetName(filter, "CFM")) == NULL)
    {
      _pdfioFileError(pdf, "Missing encryption filter method.");
      return (false);
    }
    else
    {
      if (length < 40 || length > 128)
	length = 128;			// Default to 128 bits

      if (!strcmp(cfm, "V2"))
        pdf->encryption = PDFIO_ENCRYPTION_RC4_128;
      else if (!strcmp(cfm, "AESV2"))
        pdf->encryption = PDFIO_ENCRYPTION_AES_128;
    }
  }
  else if (version == 1 || version == 2)
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
  else if (version == 6 && revision == 6)
  {
    // TODO: Implement AES-256 - V6 R6
    pdf->encryption = PDFIO_ENCRYPTION_AES_256;
    length          = 256;
  }

  PDFIO_DEBUG("_pdfioCryptoUnlock: encryption=%d, length=%d\n", pdf->encryption, length);

  if (pdf->encryption == PDFIO_ENCRYPTION_NONE)
  {
    _pdfioFileError(pdf, "Unsupported encryption V%d R%d.", version, revision);
    return (false);
  }

  // Grab the remaining values we need to unlock the PDF...
  pdf->file_keylen = (size_t)(length / 8);
  pdf->permissions = (pdfio_permission_t)pdfioDictGetNumber(encrypt_dict, "P");

  PDFIO_DEBUG("_pdfioCryptoUnlock: permissions=%d\n", pdf->permissions);

  owner_key = pdfioDictGetBinary(encrypt_dict, "O", &owner_keylen);
  user_key  = pdfioDictGetBinary(encrypt_dict, "U", &user_keylen);

  if (!owner_key)
  {
    _pdfioFileError(pdf, "Missing owner key, unable to unlock file.");
    return (false);
  }
  else if (owner_keylen < 32 || owner_keylen > sizeof(pdf->owner_key))
  {
    _pdfioFileError(pdf, "Bad %d bytes owner key, unable to unlock file.", (int)owner_keylen);
    return (false);
  }

  PDFIO_DEBUG("_pdfioCryptoUnlock: owner_key[%d]=%02X%02X%02X%02X...%02X%02X%02X%02X\n", (int)owner_keylen, owner_key[0], owner_key[1], owner_key[2], owner_key[3], owner_key[28], owner_key[29], owner_key[30], owner_key[31]);

  memcpy(pdf->owner_key, owner_key, owner_keylen);
  pdf->owner_keylen = owner_keylen;

  if (!user_key)
  {
    _pdfioFileError(pdf, "Missing user key, unable to unlock file.");
    return (false);
  }
  else if (user_keylen < 32 || user_keylen > sizeof(pdf->user_key))
  {
    _pdfioFileError(pdf, "Bad %d byte user key, unable to unlock file.", (int)user_keylen);
    return (false);
  }

  PDFIO_DEBUG("_pdfioCryptoUnlock: user_key[%d]=%02X%02X%02X%02X...%02X%02X%02X%02X\n", (int)user_keylen, user_key[0], user_key[1], user_key[2], user_key[3], user_key[28], user_key[29], user_key[30], user_key[31]);

  memcpy(pdf->user_key, user_key, user_keylen);
  pdf->user_keylen = user_keylen;

  if ((file_id = pdfioArrayGetBinary(pdf->id_array, 0, &file_idlen)) == NULL || file_idlen < 16)
  {
    _pdfioFileError(pdf, "Missing or bad file ID, unable to unlock file.");
    return (false);
  }

  // Generate a base hash from known values...
  _pdfioCryptoMD5Init(&md5);
  _pdfioCryptoMD5Append(&md5, pdf_passpad, 32);
  _pdfioCryptoMD5Append(&md5, file_id, file_idlen);
  _pdfioCryptoMD5Finish(&md5, file_digest);

  // Now try to unlock the PDF...
  for (tries = 0; tries < 4; tries ++)
  {
    if (pdf->encryption <= PDFIO_ENCRYPTION_AES_128)
    {
      uint8_t	pad[32],		// Padded password
		file_key[16],		// File key
		user_pad[32],		// Padded user password
		own_user_key[32],	// Calculated user key
		pdf_user_key[32];	// Decrypted user key

      // Pad the supplied password, if any...
      pad_password(password, pad);

      // Generate keys to see if things match...
      PDFIO_DEBUG("_pdfioCryptoUnlock: Trying %02X%02X%02X%02X...%02X%02X%02X%02X\n", pad[0], pad[1], pad[2], pad[3], pad[28], pad[29], pad[30], pad[31]);
      PDFIO_DEBUG("_pdfioCryptoUnlock: P=%d\n", pdf->permissions);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Fid(%d)=%02X%02X%02X%02X...%02X%02X%02X%02X\n", (int)file_idlen, file_id[0], file_id[1], file_id[2], file_id[3], file_id[12], file_id[13], file_id[14], file_id[15]);

      make_owner_key(pdf->encryption, pad, pdf->owner_key, user_pad);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Upad=%02X%02X%02X%02X...%02X%02X%02X%02X\n", user_pad[0], user_pad[1], user_pad[2], user_pad[3], user_pad[28], user_pad[29], user_pad[30], user_pad[31]);

      make_file_key(pdf->encryption, pdf->permissions, file_id, file_idlen, user_pad, pdf->owner_key, file_key);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Fown=%02X%02X%02X%02X...%02X%02X%02X%02X\n", file_key[0], file_key[1], file_key[2], file_key[3], file_key[12], file_key[13], file_key[14], file_key[15]);

      make_user_key(file_id, file_idlen, own_user_key);

      PDFIO_DEBUG("_pdfioCryptoUnlock: U=%02X%02X%02X%02X...%02X%02X%02X%02X\n", pdf->user_key[0], pdf->user_key[1], pdf->user_key[2], pdf->user_key[3], pdf->user_key[28], pdf->user_key[29], pdf->user_key[30], pdf->user_key[31]);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Uown=%02X%02X%02X%02X...%02X%02X%02X%02X\n", own_user_key[0], own_user_key[1], own_user_key[2], own_user_key[3], own_user_key[28], own_user_key[29], own_user_key[30], own_user_key[31]);

      if (!memcmp(own_user_key, pdf->user_key, sizeof(own_user_key)))
      {
        // Matches!
        memcpy(pdf->file_key, file_key, sizeof(pdf->file_key));
        return (true);
      }

     /*
      * Not the owner password, try the user password...
      */

      make_file_key(pdf->encryption, pdf->permissions, file_id, file_idlen, pad, pdf->owner_key, file_key);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Fuse=%02X%02X%02X%02X...%02X%02X%02X%02X\n", file_key[0], file_key[1], file_key[2], file_key[3], file_key[12], file_key[13], file_key[14], file_key[15]);

      make_user_key(file_id, file_idlen, user_key);

      memcpy(pdf_user_key, pdf->user_key, sizeof(pdf_user_key));
      decrypt_user_key(pdf->encryption, file_key, pdf_user_key);

      PDFIO_DEBUG("_pdfioCryptoUnlock: Uuse=%02X%02X%02X%02X...%02X%02X%02X%02X\n", user_key[0], user_key[1], user_key[2], user_key[3], user_key[28], user_key[29], user_key[30], user_key[31]);
      PDFIO_DEBUG("_pdfioCryptoUnlock: Updf=%02X%02X%02X%02X...%02X%02X%02X%02X\n", pdf_user_key[0], pdf_user_key[1], pdf_user_key[2], pdf_user_key[3], pdf_user_key[28], pdf_user_key[29], pdf_user_key[30], pdf_user_key[31]);

      if (!memcmp(pad, pdf_user_key, 32) || !memcmp(user_key, pdf_user_key, 16))
      {
        // Matches!
        memcpy(pdf->file_key, file_key, sizeof(pdf->file_key));
        return (true);
      }
    }
    else
    {
      // TODO: Implement AES-256 security handler
      _pdfioFileError(pdf, "Unable to unlock AES-256 encrypted file at this time.");
      return (false);
    }

    // If we get here we need to try another password...
    if (password_cb)
      password = (password_cb)(password_data, pdf->filename);

    if (!password)
      break;
  }

  _pdfioFileError(pdf, "Unable to unlock PDF file.");

  return (false);
}


//
// 'decrypt_user_key()' - Decrypt the user key.
//

static void
decrypt_user_key(
    pdfio_encryption_t encryption,	// I  - Type of encryption
    const uint8_t      *file_key,	// I  - File encryption key
    uint8_t            user_key[32])	// IO - User key
{
  size_t	i, j;			// Looping vars
  _pdfio_rc4_t	rc4;			// RC4 encryption context


  if (encryption == PDFIO_ENCRYPTION_RC4_40)
  {
    // Encrypt the result once...
    _pdfioCryptoRC4Init(&rc4, file_key, 5);
    _pdfioCryptoRC4Crypt(&rc4, user_key, user_key, 32);
  }
  else
  {
    // Encrypt the result 20 times...
    uint8_t	key[16];		// Current encryption key

    for (i = 19; i > 0; i --)
    {
      // XOR each byte in the key with the loop counter...
      for (j = 0; j < 16; j ++)
	key[j] = (uint8_t)(file_key[j] ^ i);

      _pdfioCryptoRC4Init(&rc4, key, 16);
      _pdfioCryptoRC4Crypt(&rc4, user_key, user_key, 32);
    }

    _pdfioCryptoRC4Init(&rc4, file_key, 16);
    _pdfioCryptoRC4Crypt(&rc4, user_key, user_key, 32);
  }
}


//
// 'encrypt_user_key()' - Encrypt the user key.
//

static void
encrypt_user_key(
    pdfio_encryption_t encryption,	// I  - Type of encryption
    const uint8_t      *file_key,	// I  - File encryption key
    uint8_t            user_key[32])	// IO - User key
{
  size_t	i, j;			// Looping vars
  _pdfio_rc4_t	rc4;			// RC4 encryption context


  if (encryption == PDFIO_ENCRYPTION_RC4_40)
  {
    // Encrypt the result once...
    _pdfioCryptoRC4Init(&rc4, file_key, 5);
    _pdfioCryptoRC4Crypt(&rc4, user_key, user_key, 32);
  }
  else
  {
    // Encrypt the result 20 times...
    uint8_t	key[16];		// Current encryption key

    for (i = 0; i < 20; i ++)
    {
      // XOR each byte in the key with the loop counter...
      for (j = 0; j < 16; j ++)
	key[j] = (uint8_t)(file_key[j] ^ i);

      _pdfioCryptoRC4Init(&rc4, key, 16);
      _pdfioCryptoRC4Crypt(&rc4, user_key, user_key, 32);
    }
  }
}


//
// 'make_file_key()' - Make the file encryption key.
//

static void
make_file_key(
    pdfio_encryption_t  encryption,	// I - Type of encryption
    pdfio_permission_t  permissions,	// I - File permissions
    const unsigned char *file_id,	// I - File ID value
    size_t              file_idlen,	// I - Length of file ID
    const uint8_t       *user_pad,	// I - Padded user password
    const uint8_t       *owner_key,	// I - Owner key
    uint8_t             file_key[16])	// O - Encryption key
{
  size_t	i;			// Looping var
  uint8_t	perm_bytes[4];		// Permissions bytes
  _pdfio_md5_t	md5;			// MD5 context
  uint8_t	digest[16];		// 128-bit MD5 digest


  perm_bytes[0] = (uint8_t)permissions;
  perm_bytes[1] = (uint8_t)(permissions >> 8);
  perm_bytes[2] = (uint8_t)(permissions >> 16);
  perm_bytes[3] = (uint8_t)(permissions >> 24);

  _pdfioCryptoMD5Init(&md5);
  _pdfioCryptoMD5Append(&md5, user_pad, 32);
  _pdfioCryptoMD5Append(&md5, owner_key, 32);
  _pdfioCryptoMD5Append(&md5, perm_bytes, 4);
  _pdfioCryptoMD5Append(&md5, file_id, file_idlen);
  _pdfioCryptoMD5Finish(&md5, digest);

  if (encryption != PDFIO_ENCRYPTION_RC4_40)
  {
    // MD5 the result 50 times..
    for (i = 0; i < 50; i ++)
    {
      _pdfioCryptoMD5Init(&md5);
      _pdfioCryptoMD5Append(&md5, digest, 16);
      _pdfioCryptoMD5Finish(&md5, digest);
    }
  }

  memcpy(file_key, digest, 16);
}


//
// 'make_owner_key()' - Generate the (encrypted) owner key...
//

static void
make_owner_key(
    pdfio_encryption_t encryption,	// I - Type of encryption
    const uint8_t      *owner_pad,	// I - Padded owner password
    const uint8_t      *user_pad,	// I - Padded user password
    uint8_t            owner_key[32])	// O - Owner key value
{
  size_t	i, j;			// Looping vars
  _pdfio_md5_t	md5;			// MD5 context
  uint8_t	digest[16];		// 128-bit MD5 digest
  _pdfio_rc4_t	rc4;			// RC4 encryption context


  // Hash the owner password...
  _pdfioCryptoMD5Init(&md5);
  _pdfioCryptoMD5Append(&md5, owner_pad, 32);
  _pdfioCryptoMD5Finish(&md5, digest);

  if (encryption != PDFIO_ENCRYPTION_RC4_40)
  {
    for (i = 0; i < 50; i ++)
    {
      _pdfioCryptoMD5Init(&md5);
      _pdfioCryptoMD5Append(&md5, digest, 16);
      _pdfioCryptoMD5Finish(&md5, digest);
    }
  }

  // Copy and encrypt the padded user password...
  memcpy(owner_key, user_pad, 32);

  if (encryption == PDFIO_ENCRYPTION_RC4_40)
  {
    // Encrypt once...
    _pdfioCryptoRC4Init(&rc4, digest, 5);
    _pdfioCryptoRC4Crypt(&rc4, owner_key, owner_key, 32);
  }
  else
  {
    // Encrypt 20 times...
    uint8_t encrypt_key[16];		// RC4 encryption key

    for (i = 0; i < 20; i ++)
    {
      // XOR each byte in the digest with the loop counter to make a key...
      for (j = 0; j < sizeof(encrypt_key); j ++)
	encrypt_key[j] = (uint8_t)(digest[j] ^ i);

      _pdfioCryptoRC4Init(&rc4, encrypt_key, sizeof(encrypt_key));
      _pdfioCryptoRC4Crypt(&rc4, owner_key, owner_key, 32);
    }
  }
}


//
// 'make_user_key()' - Make the user key.
//

static void
make_user_key(
    const unsigned char *file_id,	// I - File ID value
    size_t              file_idlen,	// I - Length of file ID
    uint8_t             user_key[32])	// O - User key
{
  _pdfio_md5_t	md5;			// MD5 context


  // Generate a base hash from known values...
  _pdfioCryptoMD5Init(&md5);
  _pdfioCryptoMD5Append(&md5, pdf_passpad, 32);
  _pdfioCryptoMD5Append(&md5, file_id, file_idlen);
  _pdfioCryptoMD5Finish(&md5, user_key);

  memset(user_key + 16, 0, 16);
}


//
// 'pad_password()' - Generate a padded password.
//

static void
pad_password(const char *password,	// I - Password string or `NULL`
             uint8_t    pad[32])	// O - Padded password
{
  size_t	len;			// Length of password


  if (password)
  {
    // Use the specified password
    if ((len = strlen(password)) > 32)
      len = 32;
  }
  else
  {
    // No password
    len = 0;
  }

  if (len > 0)
    memcpy(pad, password, len);
  if (len < 32)
    memcpy(pad + len, pdf_passpad, 32 - len);
}
