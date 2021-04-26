//
// Private header file for pdfio.
//
// Copyright © 2021 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef PDFIO_PRIVATE_H
#  define PDFIO_PRIVATE_H

//
// Include necessary headers...
//

#  include "pdfio.h"
#  include <stdarg.h>
#  include <errno.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <string.h>
#  include <ctype.h>
#  include <zlib.h>


//
// Visibility and other annotations...
//

#  if defined(__has_extension) || defined(__GNUC__)
#    define PDFIO_INTERNAL	__attribute__ ((visibility("hidden")))
#    define PDFIO_PRIVATE	__attribute__ ((visibility("default")))
#    define PDFIO_NONNULL(...)	__attribute__ ((nonnull(__VA_ARGS__)))
#    define PDFIO_NORETURN	__attribute__ ((noreturn))
#  else
#    define PDFIO_INTERNAL
#    define PDFIO_PRIVATE
#    define PDFIO_NONNULL(...)
#    define PDFIO_NORETURN
#  endif // __has_extension || __GNUC__


//
// Debug macro...
//

#  ifdef DEBUG
#    define PDFIO_DEBUG(...)	fprintf(stderr, __VA_ARGS__)
#  else
#    define PDFIO_DEBUG(...)
#  endif // DEBUG


//
// Types and constants...
//

struct _pdfio_file_s			// PDF file structure
{
  const char	*filename;		// Filename
  pdfio_mode_t	mode;			// Read/write mode
  pdfio_error_cb_t error_cb;		// Error callback
  void		*error_data;		// Data for error callback

  // Active file data
  int		fd;			// File descriptor
  char		buffer[8192],		// Read/write buffer
		*bufptr,		// Pointer into buffer
		*bufend;		// End of buffer
  off_t		bufpos;			// Position in file for start of buffer

  // Allocated data elements
  size_t	num_arrays,		// Number of arrays
		alloc_arrays;		// Allocated arrays
  pdfio_array_t	**arrays;		// Arrays
  size_t	num_dicts,		// Number of dictionaries
		alloc_dicts;		// Allocated dictionaries
  pdfio_dict_t	**dicts;		// Dictionaries
  size_t	num_objs,		// Number of objects
		alloc_objs;		// Allocated objects
  pdfio_obj_t	**objs;			// Objects
  size_t	num_strings,		// Number of strings
		alloc_strings;		// Allocated strings
  char		**strings;		// Nul-terminated strings
};

struct _pdfio_obj_s			// Object
{
  pdfio_file_t	*pdf;			// PDF file
  int		number,			// Number
		generation;		// Generation
  off_t		dict_offset,		// Offset to dict in file
		length_offset,		// Offset to /Length in dict
		stream_offset;		// Offset to start of stream in file
  size_t	stream_length;		// Length of stream, if any
  pdfio_dict_t	*dict;			// Dictionary
  pdfio_stream_t *stream;		// Open stream, if any
};

struct _pdfio_stream_s			// Stream
{
  pdfio_file_t	*pdf;			// PDF file
  pdfio_obj_t	*obj;			// Object
  pdfio_filter_t filter;		// Compression/decompression filter
  char		buffer[8192];		// Read/write buffer
  size_t	bufused;		// Number of bytes in buffer
  z_stream	flate;			// Flate filter state
};

typedef struct _pdfio_value_s		// Value structure
{
  pdfio_valtype_t type;			// Type of value
  union
  {
    pdfio_array_t *array;		// Array value
    bool	boolean;		// Boolean value
    time_t	date;			// Date/time value
    pdfio_dict_t *dict;			// Dictionary value
    pdfio_obj_t	*obj;			// Indirect object (N G obj) value
    const char	*name;			// Name value
    float	number;			// Number value
    const char	*string;		// String value
  }		value;			// Value union
} _pdfio_value_t;


//
// Functions...
//

extern void		_pdfioArrayDelete(pdfio_array_t *a) PDFIO_INTERNAL;
extern _pdfio_value_t	*_pdfioArrayGetValue(pdfio_array_t *a, size_t n) PDFIO_INTERNAL;

extern void		_pdfioDictDelete(pdfio_dict_t *dict) PDFIO_INTERNAL;
extern _pdfio_value_t	*_pdfioDictGetValue(pdfio_dict_t *dict, const char *key) PDFIO_INTERNAL;
extern bool		_pdfioDictSetValue(pdfio_dict_t *dict, const char *key, _pdfio_value_t *value) PDFIO_INTERNAL;

extern bool		_pdfioFileDefaultError(pdfio_file_t *pdf, const char *message, void *data) PDFIO_INTERNAL;
extern void		_pdfioFileDelete(pdfio_file_t *pdf) PDFIO_INTERNAL;
extern bool		_pdfioFileError(pdfio_file_t *pdf, const char *format, ...) PDFIO_FORMAT(2,3) PDFIO_INTERNAL;
extern int		_pdfioFileGetChar(pdfio_file_t *pdf) PDFIO_INTERNAL;
extern ssize_t		_pdfioFileRead(pdfio_file_t *pdf, char *buffer, size_t bytes) PDFIO_INTERNAL;
extern off_t		_pdfioFileSeek(pdfio_file_t *pdf, off_t offset, int whence) PDFIO_INTERNAL;
extern off_t		_pdfioFileTell(pdfio_file_t *pdf) PDFIO_INTERNAL;
extern bool		_pdfioFileWrite(pdfio_file_t *pdf, const char *buffer, size_t bytes) PDFIO_INTERNAL;

extern void		_pdfioObjDelete(pdfio_obj_t *obj) PDFIO_INTERNAL;

extern void		_pdfioStreamDelete(pdfio_stream_t *obj) PDFIO_INTERNAL;

extern bool		_pdfioStringIsAllocated(pdfio_file_t *pdf, const char *s) PDFIO_INTERNAL;

extern void		_pdfioValueDelete(_pdfio_value_t *v) PDFIO_INTERNAL;


#endif // !PDFIO_PRIVATE_H
