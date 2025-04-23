//
// Private header file for PDFio.
//
// Copyright © 2021-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_PRIVATE_H
#  define PDFIO_PRIVATE_H
#  ifdef _WIN32
#    define _CRT_SECURE_NO_WARNINGS 1	// Disable bogus VS warnings/errors...
#  endif // _WIN32
#  include "pdfio.h"
#  include <stdarg.h>
#  include <stdint.h>
#  include <string.h>
#  include <ctype.h>
#  include <errno.h>
#  include <inttypes.h>
#  include <fcntl.h>
#  include <locale.h>
#  ifdef _WIN32
#    include <io.h>
#    include <direct.h>
#    include <windows.h> // GetTempPathA
#    define access	_access		// Map standard POSIX/C99 names
#    define close	_close
#    define fileno	_fileno
#    define lseek(f,o,w) (off_t)_lseek((f),(long)(o),(w))
#    define mkdir(d,p)	_mkdir(d)
#    define open	_open
#    define read(f,b,s)	_read((f),(b),(unsigned)(s))
#    define rmdir	_rmdir
#    define snprintf	_snprintf
#    define strdup	_strdup
#    define unlink	_unlink
#    define vsnprintf	_vsnprintf
#    define write(f,b,s) _write((f),(b),(unsigned)(s))
#    ifndef F_OK
#      define F_OK	00		// POSIX parameters/flags
#      define W_OK	02
#      define R_OK	04
#    endif // !F_OK
#    define O_RDONLY	_O_RDONLY	// Map standard POSIX open flags
#    define O_WRONLY	_O_WRONLY
#    define O_CREAT	_O_CREAT
#    define O_TRUNC	_O_TRUNC
#    define O_BINARY	_O_BINARY
#  else // !_WIN32
#    include <unistd.h>
#    define O_BINARY	0		// Map Windows-specific open flag
#  endif // _WIN32
#  include <zlib.h>


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define _PDFIO_INTERNAL	__attribute__ ((visibility("hidden")))
#    define _PDFIO_PRIVATE	__attribute__ ((visibility("default")))
#    define _PDFIO_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define _PDFIO_NORETURN	__attribute__ ((noreturn))
#  else
#    define _PDFIO_INTERNAL
#    define _PDFIO_PRIVATE
#    define _PDFIO_NONNULL(...)
#    define _PDFIO_NORETURN
#  endif // __has_extension || __GNUC__


//
// Debug macro...
//

#  ifdef DEBUG
#    define PDFIO_DEBUG(...)		fprintf(stderr, __VA_ARGS__)
#    define PDFIO_DEBUG_ARRAY(array)	_pdfioArrayDebug(array, stderr)
#    define PDFIO_DEBUG_DICT(dict)	_pdfioDictDebug(dict, stderr)
#    define PDFIO_DEBUG_VALUE(value)	_pdfioValueDebug(value, stderr)
#  else
#    define PDFIO_DEBUG(...)
#    define PDFIO_DEBUG_ARRAY(array)
#    define PDFIO_DEBUG_DICT(dict)
#    define PDFIO_DEBUG_VALUE(value)
#  endif // DEBUG


//
// Types and constants...
//

#  define PDFIO_MAX_DEPTH	32	// Maximum nesting depth for values
#  define PDFIO_MAX_STRING	65536	// Maximum length of string

typedef void (*_pdfio_extfree_t)(void *);
					// Extension data free function

typedef enum _pdfio_mode_e		// Read/write mode
{
  _PDFIO_MODE_READ,			// Read a PDF file
  _PDFIO_MODE_WRITE			// Write a PDF file
} _pdfio_mode_t;

typedef enum _pdfio_predictor_e		// PNG predictor constants
{
  _PDFIO_PREDICTOR_NONE = 1,		// No predictor (default)
  _PDFIO_PREDICTOR_TIFF2 = 2,		// TIFF predictor 2 (difference from left neighbor)
  _PDFIO_PREDICTOR_PNG_NONE = 10,	// PNG None predictor (same as `_PDFIO_PREDICTOR_NONE`)
  _PDFIO_PREDICTOR_PNG_SUB = 11,	// PNG Sub predictor
  _PDFIO_PREDICTOR_PNG_UP = 12,		// PNG Up predictor
  _PDFIO_PREDICTOR_PNG_AVERAGE = 13,	// PNG Average predictor
  _PDFIO_PREDICTOR_PNG_PAETH = 14,	// PNG Paeth predictor
  _PDFIO_PREDICTOR_PNG_AUTO = 15	// PNG "auto" predictor (currently mapped to Paeth)
} _pdfio_predictor_t;

typedef ssize_t (*_pdfio_tconsume_cb_t)(void *data, size_t bytes);
typedef ssize_t (*_pdfio_tpeek_cb_t)(void *data, void *buffer, size_t bytes);

typedef struct _pdfio_token_s		// Token buffer/stack
{
  pdfio_file_t		*pdf;		// PDF file
  _pdfio_tconsume_cb_t	consume_cb;	// Consume callback
  _pdfio_tpeek_cb_t	peek_cb;	// Peek callback
  void			*cb_data;	// Callback data
  unsigned char		buffer[256],	// Buffer
			*bufptr,	// Pointer into buffer
			*bufend;	// Last valid byte in buffer
  size_t		num_tokens;	// Number of tokens in stack
  char			*tokens[4];	// Token stack
} _pdfio_token_t;

typedef struct _pdfio_value_s		// Value structure
{
  pdfio_valtype_t type;			// Type of value
  union
  {
    pdfio_array_t *array;		// Array value
    struct
    {
      unsigned char	*data;		// Data
      size_t		datalen;	// Length
    }		binary;			// Binary ("Hex String") data
    bool	boolean;		// Boolean value
    time_t	date;			// Date/time value
    pdfio_dict_t *dict;			// Dictionary value
    struct
    {
      size_t		number;		// Object number
      unsigned short	generation;	// Generation number
    }		indirect;		// Indirect object reference
    const char	*name;			// Name value
    double	number;			// Number value
    const char	*string;		// String value
  }		value;			// Value union
} _pdfio_value_t;

typedef struct _pdfio_aes_s		// AES encryption state
{
  size_t	round_size;		// Size of round key
  uint8_t	round_key[240],		// Round key
		iv[16];			// Initialization vector
} _pdfio_aes_t;

typedef struct _pdfio_md5_s		// MD5 hash state
{
  uint32_t	count[2];		// Message length in bits, lsw first
  uint32_t	abcd[4];		// Digest buffer
  uint8_t	buf[64];		// Accumulate block
} _pdfio_md5_t;

typedef struct _pdfio_rc4_s		// RC4 encryption state
{
  uint8_t	sbox[256];		// S boxes for encryption
  uint8_t	i, j;			// Current indices into S boxes
} _pdfio_rc4_t;

typedef struct _pdfio_sha265_s		// SHA-256 hash state
{
  uint32_t	Intermediate_Hash[8];	// Message Digest
  uint32_t	Length_High;		// Message length in bits
  uint32_t	Length_Low;		// Message length in bits
  int		Message_Block_Index;	// Message_Block array index
  uint8_t	Message_Block[64];	// 512-bit message blocks
  int		Computed;		// Is the hash computed?
  int		Corrupted;		// Cumulative corruption code
} _pdfio_sha256_t;

typedef union _pdfio_crypto_ctx_u	// Cryptographic contexts
{
  _pdfio_aes_t	aes;			// AES-128/256 context
  _pdfio_rc4_t	rc4;			// RC4-40/128 context
} _pdfio_crypto_ctx_t;
typedef size_t (*_pdfio_crypto_cb_t)(_pdfio_crypto_ctx_t *ctx, uint8_t *outbuffer, const uint8_t *inbuffer, size_t len);

struct _pdfio_array_s
{
  pdfio_file_t	*pdf;			// PDF file
  size_t	num_values,		// Number of values in use
		alloc_values;		// Number of allocated values
  _pdfio_value_t *values;		// Array of values
};

typedef struct _pdfio_pair_s		// Key/value pair
{
  const char	*key;			// Key string
  _pdfio_value_t value;			// Value
} _pdfio_pair_t;

struct _pdfio_dict_s			// Dictionary
{
  pdfio_file_t	*pdf;			// PDF file
  size_t	num_pairs,		// Number of pairs in use
		alloc_pairs;		// Number of allocated pairs
  _pdfio_pair_t *pairs;			// Array of pairs
};

typedef struct _pdfio_objmap_s		// PDF object map
{
  pdfio_obj_t	*obj;			// Object for this file
  pdfio_file_t	*src_pdf;		// Source PDF file
  size_t	src_number;		// Source object number
} _pdfio_objmap_t;

typedef struct _pdfio_strbuf_s		// PDF string buffer
{
  struct _pdfio_strbuf_s *next;		// Next string buffer
  bool		bufused;		// Is this string buffer being used?
  char		buffer[PDFIO_MAX_STRING + 32];
					// String buffer
} _pdfio_strbuf_t;

struct _pdfio_file_s			// PDF file structure
{
  char		*filename;		// Filename
  struct lconv	*loc;			// Locale data
  char		*version;		// Version number
  pdfio_rect_t	media_box,		// Default MediaBox value
		crop_box;		// Default CropBox value
  _pdfio_mode_t	mode;			// Read/write mode
  pdfio_output_cb_t output_cb;		// Output callback
  void		*output_ctx;		// Context for output callback
  pdfio_error_cb_t error_cb;		// Error callback
  void		*error_data;		// Data for error callback

  pdfio_encryption_t encryption;	// Encryption mode
  pdfio_permission_t permissions;	// Access permissions (encrypted PDF files)
  uint8_t	file_key[16],		// File encryption key
		owner_key[32],		// Owner encryption key
		user_key[32],		// User encryption key
		password[32];		// Padded password
  size_t	file_keylen,		// Length of file encryption key
		owner_keylen,		// Length of owner encryption key
		user_keylen;		// Length of user encryption key

  // Active file data
  int		fd;			// File descriptor
  char		buffer[8192],		// Read/write buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// End of buffer
  off_t		bufpos;			// Position in file for start of buffer
  pdfio_dict_t	*trailer_dict;		// Trailer dictionary
  pdfio_obj_t	*root_obj;		// Root object/dictionary
  pdfio_obj_t	*info_obj;		// Information object
  pdfio_obj_t	*pages_obj;		// Root pages object
  pdfio_obj_t	*encrypt_obj;		// De/Encryption object/dictionary
  pdfio_obj_t	*cgats001_obj,		// CGATS001 ICC profile object
		*cp1252_obj,		// CP1252 font encoding object
		*unicode_obj;		// Unicode font encoding object
  pdfio_array_t	*id_array;		// ID array
  bool		encrypt_metadata;	// Encrypt metadata?

  // Allocated data elements
  size_t	num_arrays,		// Number of arrays
		alloc_arrays;		// Allocated arrays
  pdfio_array_t	**arrays;		// Arrays
  size_t	num_dicts,		// Number of dictionaries
		alloc_dicts;		// Allocated dictionaries
  pdfio_dict_t	**dicts;		// Dictionaries
  size_t	num_objs,		// Number of objects
		alloc_objs,		// Allocated objects
		last_obj;		// Last object added
  pdfio_obj_t	**objs,			// Objects
		*current_obj;		// Current object being written/read
  size_t	num_objmaps,		// Number of object maps
		alloc_objmaps;		// Allocated object maps
  _pdfio_objmap_t *objmaps;		// Object maps
  size_t	num_pages,		// Number of pages
		alloc_pages;		// Allocated pages
  pdfio_obj_t	**pages;		// Pages
  size_t	num_strings,		// Number of strings
		alloc_strings;		// Allocated strings
  char		**strings;		// Nul-terminated strings
  _pdfio_strbuf_t *strbuffers;		// String buffers
};

struct _pdfio_obj_s			// Object
{
  pdfio_file_t	*pdf;			// PDF file
  size_t	number;			// Number
  unsigned short generation;		// Generation
  off_t		offset,			// Offset to object in file
		length_offset,		// Offset to /Length in object dict
		stream_offset;		// Offset to start of stream in file
  size_t	stream_length;		// Length of stream, if any
  _pdfio_value_t value;			// Dictionary/number/etc. value
  pdfio_stream_t *stream;		// Open stream, if any
  void		*data;			// Extension data, if any
  _pdfio_extfree_t datafree;		// Free callback for extension data
};

struct _pdfio_stream_s			// Stream
{
  pdfio_file_t	*pdf;			// PDF file
  pdfio_obj_t	*obj;			// Object
  pdfio_obj_t	*length_obj;		// Length object, if any
  pdfio_filter_t filter;		// Compression/decompression filter
  size_t	remaining;		// Remaining bytes in stream
  char		buffer[8192],		// Read/write buffer
		*bufptr,		// Current position in buffer
	        *bufend;		// End of buffer
  z_stream	flate;			// Flate filter state
  _pdfio_predictor_t predictor;		// Predictor function, if any
  size_t	pbpixel,		// Size of a pixel in bytes
		pbsize,			// Predictor buffer size, if any
		cbsize;			// Compressed data buffer size
  unsigned char	*cbuffer,		// Compressed data buffer
		*prbuffer,		// Raw buffer (previous line), as needed
		*psbuffer;		// PNG filter buffer, as needed
  _pdfio_crypto_cb_t crypto_cb;		// Encryption/descryption callback, if any
  _pdfio_crypto_ctx_t crypto_ctx;	// Cryptographic context
};


//
// Functions...
//

extern size_t		_pdfio_strlcpy(char *dst, const char *src, size_t dstsize) _PDFIO_INTERNAL;
extern double		_pdfio_strtod(pdfio_file_t *pdf, const char *s) _PDFIO_INTERNAL;
extern void		_pdfio_utf16cpy(char *dst, const unsigned char *src, size_t srclen, size_t dstsize) _PDFIO_INTERNAL;
extern ssize_t		_pdfio_vsnprintf(pdfio_file_t *pdf, char *buffer, size_t bufsize, const char *format, va_list ap) _PDFIO_INTERNAL;

extern bool		_pdfioArrayDecrypt(pdfio_file_t *pdf, pdfio_obj_t *obj, pdfio_array_t *a, size_t depth) _PDFIO_INTERNAL;
extern void		_pdfioArrayDebug(pdfio_array_t *a, FILE *fp) _PDFIO_INTERNAL;
extern void		_pdfioArrayDelete(pdfio_array_t *a) _PDFIO_INTERNAL;
extern _pdfio_value_t	*_pdfioArrayGetValue(pdfio_array_t *a, size_t n) _PDFIO_INTERNAL;
extern pdfio_array_t	*_pdfioArrayRead(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_token_t *ts, size_t depth) _PDFIO_INTERNAL;
extern bool		_pdfioArrayWrite(pdfio_array_t *a, pdfio_obj_t *obj) _PDFIO_INTERNAL;

extern void		_pdfioCryptoAESInit(_pdfio_aes_t *ctx, const uint8_t *key, size_t keylen, const uint8_t *iv) _PDFIO_INTERNAL;
extern size_t		_pdfioCryptoAESDecrypt(_pdfio_aes_t *ctx, uint8_t *outbuffer, const uint8_t *inbuffer, size_t len) _PDFIO_INTERNAL;
extern size_t		_pdfioCryptoAESEncrypt(_pdfio_aes_t *ctx, uint8_t *outbuffer, const uint8_t *inbuffer, size_t len) _PDFIO_INTERNAL;
extern bool		_pdfioCryptoLock(pdfio_file_t *pdf, pdfio_permission_t permissions, pdfio_encryption_t encryption, const char *owner_password, const char *user_password) _PDFIO_INTERNAL;
extern void		_pdfioCryptoMakeRandom(uint8_t *buffer, size_t bytes) _PDFIO_INTERNAL;
extern _pdfio_crypto_cb_t _pdfioCryptoMakeReader(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_crypto_ctx_t *ctx, uint8_t *iv, size_t *ivlen) _PDFIO_INTERNAL;
extern _pdfio_crypto_cb_t _pdfioCryptoMakeWriter(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_crypto_ctx_t *ctx, uint8_t *iv, size_t *ivlen) _PDFIO_INTERNAL;
extern void		_pdfioCryptoMD5Append(_pdfio_md5_t *pms, const uint8_t *data, size_t nbytes) _PDFIO_INTERNAL;
extern void		_pdfioCryptoMD5Finish(_pdfio_md5_t *pms, uint8_t digest[16]) _PDFIO_INTERNAL;
extern void		_pdfioCryptoMD5Init(_pdfio_md5_t *pms) _PDFIO_INTERNAL;
extern void		_pdfioCryptoRC4Init(_pdfio_rc4_t *ctx, const uint8_t *key, size_t keylen) _PDFIO_INTERNAL;
extern size_t		_pdfioCryptoRC4Crypt(_pdfio_rc4_t *ctx, uint8_t *outbuffer, const uint8_t *inbuffer, size_t len) _PDFIO_INTERNAL;
extern void		_pdfioCryptoSHA256Append(_pdfio_sha256_t *, const uint8_t *bytes, size_t bytecount) _PDFIO_INTERNAL;
extern void		_pdfioCryptoSHA256Init(_pdfio_sha256_t *ctx) _PDFIO_INTERNAL;
extern void		_pdfioCryptoSHA256Finish(_pdfio_sha256_t *ctx, uint8_t *Message_Digest) _PDFIO_INTERNAL;
extern bool		_pdfioCryptoUnlock(pdfio_file_t *pdf, pdfio_password_cb_t password_cb, void *password_data) _PDFIO_INTERNAL;

extern bool		_pdfioDictDecrypt(pdfio_file_t *pdf, pdfio_obj_t *obj, pdfio_dict_t *dict, size_t depth) _PDFIO_INTERNAL;
extern void		_pdfioDictDebug(pdfio_dict_t *dict, FILE *fp) _PDFIO_INTERNAL;
extern void		_pdfioDictDelete(pdfio_dict_t *dict) _PDFIO_INTERNAL;
extern _pdfio_value_t	*_pdfioDictGetValue(pdfio_dict_t *dict, const char *key) _PDFIO_INTERNAL;
extern pdfio_dict_t	*_pdfioDictRead(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_token_t *ts, size_t depth) _PDFIO_INTERNAL;
extern bool		_pdfioDictSetValue(pdfio_dict_t *dict, const char *key, _pdfio_value_t *value) _PDFIO_INTERNAL;
extern bool		_pdfioDictWrite(pdfio_dict_t *dict, pdfio_obj_t *obj, off_t *length) _PDFIO_INTERNAL;

extern bool		_pdfioFileAddMappedObj(pdfio_file_t *pdf, pdfio_obj_t *dst_obj, pdfio_obj_t *src_obj) _PDFIO_INTERNAL;
extern bool		_pdfioFileAddPage(pdfio_file_t *pdf, pdfio_obj_t *obj) _PDFIO_INTERNAL;
extern bool		_pdfioFileConsume(pdfio_file_t *pdf, size_t bytes) _PDFIO_INTERNAL;
extern pdfio_obj_t	*_pdfioFileCreateObj(pdfio_file_t *pdf, pdfio_file_t *srcpdf, _pdfio_value_t *value) _PDFIO_INTERNAL;
extern bool		_pdfioFileDefaultError(pdfio_file_t *pdf, const char *message, void *data) _PDFIO_INTERNAL;
extern bool		_pdfioFileError(pdfio_file_t *pdf, const char *format, ...) _PDFIO_INTERNAL;
extern pdfio_obj_t	*_pdfioFileFindMappedObj(pdfio_file_t *pdf, pdfio_file_t *src_pdf, size_t src_number) _PDFIO_INTERNAL;
extern bool		_pdfioFileFlush(pdfio_file_t *pdf) _PDFIO_INTERNAL;
extern int		_pdfioFileGetChar(pdfio_file_t *pdf) _PDFIO_INTERNAL;
extern bool		_pdfioFileGets(pdfio_file_t *pdf, char *buffer, size_t bufsize) _PDFIO_INTERNAL;
extern ssize_t		_pdfioFilePeek(pdfio_file_t *pdf, void *buffer, size_t bytes) _PDFIO_INTERNAL;
extern bool		_pdfioFilePrintf(pdfio_file_t *pdf, const char *format, ...) _PDFIO_INTERNAL;
extern bool		_pdfioFilePuts(pdfio_file_t *pdf, const char *s) _PDFIO_INTERNAL;
extern ssize_t		_pdfioFileRead(pdfio_file_t *pdf, void *buffer, size_t bytes) _PDFIO_INTERNAL;
extern off_t		_pdfioFileSeek(pdfio_file_t *pdf, off_t offset, int whence) _PDFIO_INTERNAL;
extern off_t		_pdfioFileTell(pdfio_file_t *pdf) _PDFIO_INTERNAL;
extern bool		_pdfioFileWrite(pdfio_file_t *pdf, const void *buffer, size_t bytes) _PDFIO_INTERNAL;

extern void		_pdfioObjDelete(pdfio_obj_t *obj) _PDFIO_INTERNAL;
extern void		*_pdfioObjGetExtension(pdfio_obj_t *obj) _PDFIO_INTERNAL;
extern bool		_pdfioObjLoad(pdfio_obj_t *obj) _PDFIO_INTERNAL;
extern void		_pdfioObjSetExtension(pdfio_obj_t *obj, void *data, _pdfio_extfree_t datafree) _PDFIO_INTERNAL;
extern bool		_pdfioObjWriteHeader(pdfio_obj_t *obj) _PDFIO_INTERNAL;

extern pdfio_stream_t	*_pdfioStreamCreate(pdfio_obj_t *obj, pdfio_obj_t *length_obj, size_t cbsize, pdfio_filter_t compression) _PDFIO_INTERNAL;
extern pdfio_stream_t	*_pdfioStreamOpen(pdfio_obj_t *obj, bool decode) _PDFIO_INTERNAL;

extern char		*_pdfioStringAllocBuffer(pdfio_file_t *pdf);
extern void		_pdfioStringFreeBuffer(pdfio_file_t *pdf, char *buffer);
extern bool		_pdfioStringIsAllocated(pdfio_file_t *pdf, const char *s) _PDFIO_INTERNAL;

extern void		_pdfioTokenClear(_pdfio_token_t *tb) _PDFIO_INTERNAL;
extern void		_pdfioTokenFlush(_pdfio_token_t *tb) _PDFIO_INTERNAL;
extern bool		_pdfioTokenGet(_pdfio_token_t *tb, char *buffer, size_t bufsize) _PDFIO_INTERNAL;
extern void		_pdfioTokenInit(_pdfio_token_t *tb, pdfio_file_t *pdf, _pdfio_tconsume_cb_t consume_cb, _pdfio_tpeek_cb_t peek_cb, void *cb_data);
extern void		_pdfioTokenPush(_pdfio_token_t *tb, const char *token) _PDFIO_INTERNAL;
extern bool		_pdfioTokenRead(_pdfio_token_t *tb, char *buffer, size_t bufsize);

extern _pdfio_value_t	*_pdfioValueCopy(pdfio_file_t *pdfdst, _pdfio_value_t *vdst, pdfio_file_t *pdfsrc, _pdfio_value_t *vsrc) _PDFIO_INTERNAL;
extern bool		_pdfioValueDecrypt(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_value_t *v, size_t depth) _PDFIO_INTERNAL;
extern void		_pdfioValueDebug(_pdfio_value_t *v, FILE *fp) _PDFIO_INTERNAL;
extern void		_pdfioValueDelete(_pdfio_value_t *v) _PDFIO_INTERNAL;
extern _pdfio_value_t	*_pdfioValueRead(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_token_t *ts, _pdfio_value_t *v, size_t depth) _PDFIO_INTERNAL;
extern bool		_pdfioValueWrite(pdfio_file_t *pdf, pdfio_obj_t *obj, _pdfio_value_t *v, off_t *length) _PDFIO_INTERNAL;


#endif // !PDFIO_PRIVATE_H
