//
// Public header file for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_H
#  define PDFIO_H

//
// Include necessary headers...
//

#  include <stdio.h>
#  include <stdlib.h>
#  include <stdbool.h>
#  include <sys/types.h>
#  include <time.h>


//
// C++ magic...
//

#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define PDFIO_PUBLIC	__attribute__ ((visibility("default")))
#    define PDFIO_FORMAT(a,b)	__attribute__ ((__format__(__printf__, a,b)))
#  else
#    define PDFIO_PUBLIC
#    define PDFIO_FORMAT(a,b)
#  endif // __has_extension || __GNUC__


//
// Types and constants...
//

typedef struct _pdfio_array_s pdfio_array_t;
					// Array of PDF values
typedef struct _pdfio_dict_s pdfio_dict_t;
					// Key/value dictionary
typedef struct _pdfio_file_s pdfio_file_t;
					// PDF file
typedef bool (*pdfio_error_cb_t)(pdfio_file_t *pdf, const char *message, void *data);
					// Error callback
typedef enum pdfio_filter_e		// Compression/decompression filters for streams
{
  PDFIO_FILTER_NONE,			// No filter
  PDFIO_FILTER_ASCIIHEX,		// ASCIIHexDecode filter (reading only)
  PDFIO_FILTER_ASCII85,			// ASCII85Decode filter (reading only)
  PDFIO_FILTER_CCITTFAX,		// CCITTFaxDecode filter
  PDFIO_FILTER_CRYPT,			// Encryption filter
  PDFIO_FILTER_DCT,			// DCTDecode (JPEG) filter
  PDFIO_FILTER_FLATE,			// FlateDecode filter
  PDFIO_FILTER_JBIG2,			// JBIG2Decode filter
  PDFIO_FILTER_JPX,			// JPXDecode filter (reading only)
  PDFIO_FILTER_LZW,			// LZWDecode filter (reading only)
  PDFIO_FILTER_RUNLENGTH,		// RunLengthDecode filter (reading only)
} pdfio_filter_t;
typedef struct _pdfio_obj_s pdfio_obj_t;// Numbered object in PDF file
typedef struct pdfio_rect_s		// PDF rectangle
{
  float	x1;				// Lower-left X coordinate
  float	y1;				// Lower-left Y coordinate
  float	x2;				// Upper-right X coordinate
  float	y2;				// Upper-right Y coordinate
} pdfio_rect_t;
typedef struct _pdfio_stream_s pdfio_stream_t;
					// Object data stream in PDF file
typedef enum pdfio_valtype_e		// PDF value types
{
  PDFIO_VALTYPE_NONE,			// No value, not set
  PDFIO_VALTYPE_ARRAY,			// Array
  PDFIO_VALTYPE_BINARY,			// Binary data
  PDFIO_VALTYPE_BOOLEAN,		// Boolean
  PDFIO_VALTYPE_DATE,			// Date/time
  PDFIO_VALTYPE_DICT,			// Dictionary
  PDFIO_VALTYPE_INDIRECT,		// Indirect object (N G obj)
  PDFIO_VALTYPE_NAME,			// Name
  PDFIO_VALTYPE_NULL,			// Null object
  PDFIO_VALTYPE_NUMBER,			// Number (integer or real)
  PDFIO_VALTYPE_STRING			// String
} pdfio_valtype_t;
typedef struct _pdfio_value_s pdf_value_t;
					// PDF value of any type


//
// Functions...
//

extern bool		pdfioArrayAppendArray(pdfio_array_t *a, pdfio_array_t *value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendBinary(pdfio_array_t *a, unsigned char *value, size_t valuelen) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendBoolean(pdfio_array_t *a, bool value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendDict(pdfio_array_t *a, pdfio_dict_t *value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendName(pdfio_array_t *a, const char *value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendNumber(pdfio_array_t *a, float value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendObject(pdfio_array_t *a, pdfio_obj_t *value) PDFIO_PUBLIC;
extern bool		pdfioArrayAppendString(pdfio_array_t *a, const char *value) PDFIO_PUBLIC;
extern pdfio_array_t	*pdfioArrayCopy(pdfio_file_t *pdf, pdfio_array_t *a) PDFIO_PUBLIC;
extern pdfio_array_t	*pdfioArrayCreate(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern pdfio_array_t	*pdfioArrayGetArray(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern unsigned char	*pdfioArrayGetBinary(pdfio_array_t *a, size_t n, size_t *length) PDFIO_PUBLIC;
extern bool		pdfioArrayGetBoolean(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern pdfio_dict_t	*pdfioArrayGetDict(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern const char	*pdfioArrayGetName(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern float		pdfioArrayGetNumber(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioArrayGetObject(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern size_t		pdfioArrayGetSize(pdfio_array_t *a) PDFIO_PUBLIC;
extern const char	*pdfioArrayGetString(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;
extern pdfio_valtype_t	pdfioArrayGetType(pdfio_array_t *a, size_t n) PDFIO_PUBLIC;

extern pdfio_dict_t	*pdfioDictCopy(pdfio_file_t *pdf, pdfio_dict_t *dict) PDFIO_PUBLIC;
extern pdfio_dict_t	*pdfioDictCreate(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern pdfio_array_t	*pdfioDictGetArray(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern unsigned char	*pdfioDictGetBinary(pdfio_dict_t *dict, const char *key, size_t *length) PDFIO_PUBLIC;
extern bool		pdfioDictGetBoolean(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern pdfio_dict_t	*pdfioDictGetDict(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern const char	*pdfioDictGetName(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern float		pdfioDictGetNumber(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioDictGetObject(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern pdfio_rect_t	*pdfioDictGetRect(pdfio_dict_t *dict, const char *key, pdfio_rect_t *rect) PDFIO_PUBLIC;
extern const char	*pdfioDictGetString(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern pdfio_valtype_t	pdfioDictGetType(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern bool		pdfioDictSetArray(pdfio_dict_t *dict, const char *key, pdfio_array_t *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetBinary(pdfio_dict_t *dict, const char *key, unsigned char *value, size_t valuelen) PDFIO_PUBLIC;
extern bool		pdfioDictSetBoolean(pdfio_dict_t *dict, const char *key, bool value) PDFIO_PUBLIC;
extern bool		pdfioDictSetDict(pdfio_dict_t *dict, const char *key, pdfio_dict_t *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetName(pdfio_dict_t *dict, const char *key, const char *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetNull(pdfio_dict_t *dict, const char *key) PDFIO_PUBLIC;
extern bool		pdfioDictSetNumber(pdfio_dict_t *dict, const char *key, float value) PDFIO_PUBLIC;
extern bool		pdfioDictSetObject(pdfio_dict_t *dict, const char *key, pdfio_obj_t *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetRect(pdfio_dict_t *dict, const char *key, pdfio_rect_t *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetString(pdfio_dict_t *dict, const char *key, const char *value) PDFIO_PUBLIC;
extern bool		pdfioDictSetStringf(pdfio_dict_t *dict, const char *key, const char *format, ...) PDFIO_PUBLIC PDFIO_FORMAT(3,4);

extern bool		pdfioFileClose(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern pdfio_file_t	*pdfioFileCreate(const char *filename, const char *version, pdfio_error_cb_t error_cb, void *error_data) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileCreateObject(pdfio_file_t *pdf, pdfio_dict_t *dict) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileCreatePage(pdfio_file_t *pdf, pdfio_dict_t *dict) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileFindObject(pdfio_file_t *pdf, size_t number) PDFIO_PUBLIC;
extern pdfio_array_t	*pdfioFileGetID(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern const char	*pdfioFileGetName(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern size_t		pdfioFileGetNumObjects(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern size_t		pdfioFileGetNumPages(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileGetObject(pdfio_file_t *pdf, size_t n) PDFIO_PUBLIC;
extern pdfio_obj_t	*pdfioFileGetPage(pdfio_file_t *pdf, size_t n) PDFIO_PUBLIC;
extern const char	*pdfioFileGetVersion(pdfio_file_t *pdf) PDFIO_PUBLIC;
extern pdfio_file_t	*pdfioFileOpen(const char *filename, pdfio_error_cb_t error_cb, void *error_data) PDFIO_PUBLIC;

extern bool		pdfioObjClose(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern pdfio_stream_t	*pdfioObjCreateStream(pdfio_obj_t *obj, pdfio_filter_t compression) PDFIO_PUBLIC;
extern pdfio_dict_t	*pdfioObjGetDict(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern unsigned short	pdfioObjGetGeneration(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern size_t		pdfioObjGetLength(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern size_t		pdfioObjGetNumber(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern const char	*pdfioObjGetType(pdfio_obj_t *obj) PDFIO_PUBLIC;
extern pdfio_stream_t	*pdfioObjOpenStream(pdfio_obj_t *obj, bool decode) PDFIO_PUBLIC;

extern pdfio_obj_t	*pdfioPageCopy(pdfio_file_t *pdf, pdfio_obj_t *src) PDFIO_PUBLIC;

extern bool		pdfioStreamClose(pdfio_stream_t *st) PDFIO_PUBLIC;
extern bool		pdfioStreamConsume(pdfio_stream_t *st, size_t bytes) PDFIO_PUBLIC;
extern bool		pdfioStreamGetToken(pdfio_stream_t *st, char *buffer, size_t bufsize) PDFIO_PUBLIC;
extern ssize_t		pdfioStreamPeek(pdfio_stream_t *st, void *buffer, size_t bytes) PDFIO_PUBLIC;
extern bool		pdfioStreamPrintf(pdfio_stream_t *st, const char *format, ...) PDFIO_PUBLIC PDFIO_FORMAT(2,3);
extern bool		pdfioStreamPuts(pdfio_stream_t *st, const char *s) PDFIO_PUBLIC;
extern ssize_t		pdfioStreamRead(pdfio_stream_t *st, void *buffer, size_t bytes) PDFIO_PUBLIC;
extern bool		pdfioStreamWrite(pdfio_stream_t *st, const void *buffer, size_t bytes) PDFIO_PUBLIC;

extern char		*pdfioStringCreate(pdfio_file_t *pdf, const char *s)  PDFIO_PUBLIC;
extern char		*pdfioStringCreatef(pdfio_file_t *pdf, const char *format, ...) PDFIO_FORMAT(2,3) PDFIO_PUBLIC;


//
// C++ magic...
//

#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !PDFIO_H
