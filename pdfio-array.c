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


  // Range check input
  if (!a || !value)
    return (false);

  // Add an array...
  v.type        = PDFIO_VALTYPE_ARRAY;
  v.value.array = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendBinary()' - Add a binary string value to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendBinary(
    pdfio_array_t *a,			// I - Array
    unsigned char *value,		// I - Value
    size_t        valuelen)		// I - Length of value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a || !value || !valuelen)
    return (false);

  // Add a binary string...
  v.type                 = PDFIO_VALTYPE_BINARY;
  v.value.binary.datalen = valuelen;

  if ((v.value.binary.data = (unsigned char *)malloc(valuelen)) == NULL)
  {
    _pdfioFileError(a->pdf, "Unable to allocate memory for binary string - %s", strerror(errno));
    return (false);
  }

  memcpy(v.value.binary.data, value, valuelen);

  return (append_value(a, &v));

}

//
// 'pdfioArrayAppendBoolean()' - Add a boolean value to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendBoolean(
    pdfio_array_t *a,			// I - Array
    bool          value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a)
    return (false);

  // Add a boolean...
  v.type          = PDFIO_VALTYPE_BOOLEAN;
  v.value.boolean = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendDict()' - Add a dictionary to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendDict(
    pdfio_array_t *a,			// I - Array
    pdfio_dict_t  *value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a || !value)
    return (false);

  // Add a dictionary...
  v.type       = PDFIO_VALTYPE_DICT;
  v.value.dict = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendName()' - Add a name to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendName(
    pdfio_array_t *a,			// I - Array
    const char    *value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a || !value)
    return (false);

  // Add a name string...
  v.type       = PDFIO_VALTYPE_NAME;
  v.value.name = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendNumber()' - Add a number to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendNumber(
    pdfio_array_t *a,			// I - Array
    float         value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a)
    return (false);

  // Add a number...
  v.type         = PDFIO_VALTYPE_NUMBER;
  v.value.number = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendObject()' - Add an indirect object reference to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendObject(
    pdfio_array_t *a,			// I - Array
    pdfio_obj_t   *value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a || !value || a->pdf != value->pdf)
    return (false);

  // Add an indirect reference...
  v.type      = PDFIO_VALTYPE_INDIRECT;
  v.value.obj = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayAppendString()' - Add a string to an array.
//

bool					// O - `true` on success, `false` on failure
pdfioArrayAppendString(
    pdfio_array_t *a,			// I - Array
    const char    *value)		// I - Value
{
  _pdfio_value_t	v;		// Value for array


  // Range check input
  if (!a || !value)
    return (false);

  // Add a string...
  v.type         = PDFIO_VALTYPE_STRING;
  v.value.string = value;

  return (append_value(a, &v));
}


//
// 'pdfioArrayCopy()' - Copy an array.
//

pdfio_array_t *				// O - New array or `NULL` on error
pdfioArrayCopy(pdfio_file_t  *pdf,	// I - PDF file
               pdfio_array_t *a)	// I - Original array
{
  pdfio_array_t		*na;		// New array
  size_t		i;		// Looping var
  _pdfio_value_t	*vsrc,		// Current source value
			vdst;		// Current destination value


  // Create the new array...
  if ((na = pdfioArrayCreate(pdf)) == NULL)
    return (NULL);

  // Pre-allocate the values array to make this a little faster...
  if ((na->values = (_pdfio_value_t *)malloc(a->num_values * sizeof(_pdfio_value_t))) == NULL)
    return (NULL);			// Let pdfioFileClose do the cleanup...

  na->alloc_values = a->num_values;

  // Copy and add each of the source array's values...
  for (i = a->num_values, vsrc = a->values; i > 0; i --, vsrc ++)
  {
    if (!_pdfioValueCopy(pdf, &vdst, a->pdf, vsrc))
      return (NULL);			// Let pdfioFileClose do the cleanup...

    // Cannot fail since we already allocated memory...
    append_value(na, &vdst);
  }

  // Successfully copied the array, so return it...
  return (na);
}


//
// 'pdfioArrayCreate()' - Create an empty array.
//

pdfio_array_t *				// O - New array or `NULL` on error
pdfioArrayCreate(pdfio_file_t *pdf)	// I - PDF file
{
  pdfio_array_t	*a;			// Array


  if (!pdf)
    return (NULL);

  if ((a = (pdfio_array_t *)calloc(1, sizeof(pdfio_array_t))) == NULL)
    return (NULL);

  a->pdf = pdf;

  if (pdf->num_arrays >= pdf->alloc_arrays)
  {
    pdfio_array_t **temp = realloc(pdf->arrays, (pdf->alloc_arrays + 16) * sizeof(pdfio_array_t *));

    if (!temp)
    {
      free(a);
      return (NULL);
    }

    pdf->arrays       = temp;
    pdf->alloc_arrays += 16;
  }

  pdf->arrays[pdf->num_arrays ++] = a;

  return (a);
}


//
// '_pdfioArrayDelete()' - Free the memory used by an array.
//

void
_pdfioArrayDelete(pdfio_array_t *a)	// I - Array
{
  if (a)
    free(a->values);

  free(a);
}


//
// 'pdfioArrayGetArray()' - Get an array value from an array.
//

pdfio_array_t *				// O - Value
pdfioArrayGetArray(pdfio_array_t *a,	// I - Array
                   size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_ARRAY)
    return (NULL);
  else
    return (a->values[n].value.array);
}


//
// 'pdfioArrayGetBinary()' - Get a binary string value from an array.
//

unsigned char *				// O - Value
pdfioArrayGetBinary(
    pdfio_array_t *a,			// I - Array
    size_t        n,			// I - Index
    size_t        *length)		// O - Length of string
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_BINARY || !length)
  {
    if (length)
      *length = 0;

    return (NULL);
  }
  else
  {
    *length = a->values[n].value.binary.datalen;
    return (a->values[n].value.binary.data);
  }
}


//
// 'pdfioArrayGetBoolean()' - Get a boolean value from an array.
//

bool					// O - Value
pdfioArrayGetBoolean(pdfio_array_t *a,	// I - Array
                     size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_BOOLEAN)
    return (false);
  else
    return (a->values[n].value.boolean);
}


//
// 'pdfioArrayGetDict()' - Get a dictionary value from an array.
//

pdfio_dict_t *				// O - Value
pdfioArrayGetDict(pdfio_array_t *a,	// I - Array
		  size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_DICT)
    return (NULL);
  else
    return (a->values[n].value.dict);
}


//
// 'pdfioArrayGetName()' - Get a name value from an array.
//

const char *				// O - Value
pdfioArrayGetName(pdfio_array_t *a,	// I - Array
		  size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_NAME)
    return (NULL);
  else
    return (a->values[n].value.name);
}


//
// 'pdfioArrayGetNumber()' - Get a number from an array.
//

float					// O - Value
pdfioArrayGetNumber(pdfio_array_t *a,	// I - Array
                    size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_NUMBER)
    return (0.0f);
  else
    return (a->values[n].value.number);
}


//
// 'pdfioArrayGetObject()' - Get an indirect object reference from an array.
//

pdfio_obj_t *				// O - Value
pdfioArrayGetObject(pdfio_array_t *a,	// I - Array
                    size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_INDIRECT)
    return (NULL);
  else
    return (a->values[n].value.obj);
}


//
// 'pdfioArrayGetSize()' - Get the length of an array.
//

size_t					// O - Length of array
pdfioArrayGetSize(pdfio_array_t *a)	// I - Array
{
  return (a ? a->num_values : 0);
}


//
// 'pdfioArrayGetString()' - Get a string value from an array.
//

const char *				// O - Value
pdfioArrayGetString(pdfio_array_t *a,	// I - Array
                    size_t        n)	// I - Index
{
  if (!a || n >= a->num_values || a->values[n].type != PDFIO_VALTYPE_STRING)
    return (NULL);
  else
    return (a->values[n].value.string);
}


//
// 'pdfioArrayGetType()' - Get a value type from an array.
//

pdfio_valtype_t				// O - Value type
pdfioArrayGetType(pdfio_array_t *a,	// I - Array
                  size_t        n)	// I - Index
{
  if (!a || n >= a->num_values)
    return (PDFIO_VALTYPE_NONE);
  else
    return (a->values[n].type);
}


//
// '_pdfioArrayGetValue()' - Get a value from an array.
//

_pdfio_value_t *			// O - Value
_pdfioArrayGetValue(pdfio_array_t *a,	// I - Array
                    size_t        n)	// I - Index
{
  if (!a || n >= a->num_values)
    return (NULL);
  else
    return (a->values + n);
}


//
// '_pdfioArrayWrite()' - Write an array to a PDF file.
//

bool					// O - `true` on success, `false` otherwise
_pdfioArrayWrite(pdfio_array_t *a)	// I - Array
{
  pdfio_file_t	*pdf = a->pdf;		// PDF file
  size_t	i;			// Looping var
  _pdfio_value_t *v;			// Current value


  // Arrays are surrounded by square brackets ([ ... ])
  if (!_pdfioFilePuts(pdf, "["))
    return (false);

  // Write each value...
  for (i = a->num_values, v = a->values; i > 0; i --, v ++)
  {
    if (!_pdfioValueWrite(pdf, v))
      return (false);
  }

  // Closing bracket...
  return (_pdfioFilePuts(pdf, "]"));
}


//
// 'append_value()' - Append a value.
//

static bool				// O - `true` on success, `false` otherwise
append_value(pdfio_array_t  *a,		// I - Array
             _pdfio_value_t *v)		// I - Value
{
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
