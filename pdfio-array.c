//
// PDF array functions for pdfio.
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


//
// Array structure
//

struct _pdfio_array_s
{
  size_t	num_values,		// Number of values in use
		alloc_values;		// Number of allocated values
  _pdfio_value_t *values;		// Array of values
};


//
// Local functions...
//

static bool	append_value(pdfio_array_t *a, _pdfio_value_t *v);


//
// 'pdfioArrayAppendArray()' - Add an array value to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendArray(
    pdfio_array_t *a,			// I - Array
    pdfio_array_t *value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  v.type        = PDFIO_VALTYPE_ARRAY;
  v.value.array = value;

  return (append_value(a, &v));
}


//
// '()' - .
//

bool		pdfioArrayAppendBoolean(pdfio_array_t *a, boolean value)
{
}


//
// '()' - .
//

bool		pdfioArrayAppendDict(pdfio_array_t *a, pdfio_dict_t *value)
{
}


//
// '()' - .
//

bool		pdfioArrayAppendName(pdfio_array_t *a, const char *value)
{
}


//
// '()' - .
//

bool		pdfioArrayAppendNumber(pdfio_array_t *a, float value)
{
}


//
// '()' - .
//

bool		pdfioArrayAppendObject(pdfio_array_t *a, pdfio_obj_t *value)
{
}


//
// '()' - .
//

bool		pdfioArrayAppendString(pdfio_array_t *a, const char *value)
{
}


//
// '()' - .
//

pdfio_array_t	*pdfioArrayCreate(pdfio_file_t *file)
{
}


//
// '()' - .
//

void	_pdfioArrayDelete(pdfio_array_t *a)
{
}


//
// '()' - .
//

pdfio_array_t	*pdfioArrayGetArray(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

bool		pdfioArrayGetBoolean(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

pdfio_dict_t	*pdfioArrayGetDict(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

const char	*pdfioArrayGetName(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

float		pdfioArrayGetNumber(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioArrayGetObject(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

int		pdfioArrayGetSize(pdfio_array_t *a)
{
}


//
// '()' - .
//

const char	*pdfioArrayGetString(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

pdfio_valtype_t	pdfioArrayGetType(pdfio_array_t *a, int n)
{
}


//
// '()' - .
//

_pdfio_value_t	*
_pdfioArrayGetValue(pdfio_array_t *a, int n)
{
}


//
// 'append_value()' - Append a value.
//

static bool				// O - `true` on success, `false` otherwise
append_value(pdfio_array_t  *a,		// I - Array
             _pdfio_value_t *v)		// I - Value
{
  if (!a)
    return (false);

  if (a->num_values >= a->alloc_values)
  {
    _pdfio_value_t *temp = (_pdfio_value_t *)realloc(a->values, (a->alloc_values + 16) * sizeof(_pdfio_value_t));

    if (!temp)
      return (false);

    a->values       = temp;
    a->alloc_values += 16;
  }

  a->values[a->num_values ++] = *v;

  return (true);
}
