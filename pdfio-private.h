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
#  include <fcntl.h>
#  include <unistd.h>
#  include <string.h>
#  include <ctype.h>


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
  int		fd;			// File descriptor

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
  int		number,			// Number
		generation;		// Generation
  off_t		offset;			// Offset in file
  size_t	length;			// Length
  pdfio_dict_t	*dict;			// Dictionary
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
extern void		_pdfioFileDelete(pdfio_file_t *file) PDFIO_INTERNAL;
extern void		_pdfioObjDelete(pdfio_obj_t *obj) PDFIO_INTERNAL;
extern void		_pdfioStreamDelete(pdfio_stream_t *obj) PDFIO_INTERNAL;
extern bool		_pdfioStringIsAllocated(pdfio_file_t *pdf, const char *s) PDFIO_INTERNAL;
extern void		_pdfioValueDelete(_pdfio_value_t *v) PDFIO_INTERNAL;


#endif // !PDFIO_PRIVATE_H
