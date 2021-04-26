//
// PDF dictionary functions for pdfio.
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
// Dictionary structures
//

typedef struct _pdfio_pair_s		// Key/value pair
{
  const char	*key;			// Key string
  _pdfio_value_t value;			// Value
} _pdfio_pair_t;

struct _pdfio_dict_s
{
  pdfio_file_t	*pdf;			// PDF file
  size_t	num_pairs,		// Number of pairs in use
		alloc_pairs;		// Number of allocated pairs
  _pdfio_pair_t *pairs;			// Array of pairs
};


//
// Local functions...
//

static int	compare_pairs(_pdfio_pair_t *a, _pdfio_pair_t *b);


//
// 'pdfioDictCreate()' - Create a dictionary to hold key/value pairs.
//

pdfio_dict_t *				// O - New dictionary
pdfioDictCreate(pdfio_file_t *pdf)	// I - PDF file
{
  pdfio_dict_t	*dict;			// New dictionary


  if (!pdf)
    return (NULL);

  if ((dict = (pdfio_dict_t *)calloc(1, sizeof(pdfio_dict_t))) == NULL)
    return (NULL);

  dict->pdf = pdf;

  if (pdf->num_dicts >= pdf->alloc_dicts)
  {
    pdfio_dict_t **temp = (pdfio_dict_t **)realloc(pdf->dicts, (pdf->alloc_dicts + 16) * sizeof(pdfio_dict_t *));

    if (!temp)
    {
      free(dict);
      return (NULL);
    }

    pdf->dicts       = temp;
    pdf->alloc_dicts += 16;
  }

  pdf->dicts[pdf->num_dicts ++] = dict;

  return (dict);
}


//
// '_pdfioDictDelete()' - Free the memory used by a dictionary.
//

void
_pdfioDictDelete(pdfio_dict_t *dict)	// I - Dictionary
{
  if (dict)
    free(dict->pairs);

  free(dict);
}


//
// 'pdfioDictGetArray()' - Get a key array value from a dictionary.
//

pdfio_array_t *				// O - Value
pdfioDictGetArray(pdfio_dict_t *dict,	// I - Dictionary
                  const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_ARRAY)
    return (value->value.array);
  else
    return (NULL);
}


//
// 'pdfioDictGetBoolean()' - Get a key boolean value from a dictionary.
//

bool					// O - Value
pdfioDictGetBoolean(pdfio_dict_t *dict,	// I - Dictionary
                    const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_BOOLEAN)
    return (value->value.boolean);
  else
    return (false);
}


//
// 'pdfioDictGetDict()' - Get a key dictionary value from a dictionary.
//

pdfio_dict_t *				// O - Value
pdfioDictGetDict(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_DICT)
    return (value->value.dict);
  else
    return (NULL);
}


//
// 'pdfioDictGetName()' - Get a key name value from a dictionary.
//

const char *				// O - Value
pdfioDictGetName(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_NAME)
    return (value->value.name);
  else
    return (NULL);
}


//
// 'pdfioDictGetNumber()' - Get a key number value from a dictionary.
//

float					// O - Value
pdfioDictGetNumber(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_NUMBER)
    return (value->value.number);
  else
    return (0.0f);
}


//
// 'pdfioDictGetObject()' - Get a key indirect object value from a dictionary.
//

pdfio_obj_t *				// O - Value
pdfioDictGetObject(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_INDIRECT)
    return (value->value.obj);
  else
    return (NULL);
}


//
// 'pdfioDictGetRect()' - Get a key rectangle value from a dictionary.
//

pdfio_rect_t *				// O - Rectangle
pdfioDictGetRect(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key,	// I - Key
                 pdfio_rect_t *rect)	// I - Rectangle
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_ARRAY && pdfioArrayGetSize(value->value.array) == 4)
  {
    rect->x1 = pdfioArrayGetNumber(value->value.array, 0);
    rect->y1 = pdfioArrayGetNumber(value->value.array, 1);
    rect->x2 = pdfioArrayGetNumber(value->value.array, 2);
    rect->y2 = pdfioArrayGetNumber(value->value.array, 3);
    return (rect);
  }
  else
  {
    memset(rect, 0, sizeof(pdfio_rect_t));
    return (NULL);
  }
}


//
// 'pdfioDictGetString()' - Get a key string value from a dictionary.
//

const char *				// O - Value
pdfioDictGetString(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (value && value->type == PDFIO_VALTYPE_STRING)
    return (value->value.string);
  else
    return (NULL);
}


//
// 'pdfioDictGetType()' - Get a key value type from a dictionary.
//

pdfio_valtype_t				// O - Value type
pdfioDictGetType(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  return (value ? value->type : PDFIO_VALTYPE_NONE);
}


//
// '_pdfioDictGetValue()' - Get a key value from a dictionary.
//

_pdfio_value_t *			// O - Value or `NULL` on error
_pdfioDictGetValue(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key)	// I - Key
{
  _pdfio_pair_t	temp,			// Search key
		*match;			// Matching key pair


  if (!dict || !dict->num_pairs || !key)
    return (NULL);

  temp.key = key;

  if ((match = bsearch(&temp, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
    return (&match->value);
  else
    return (NULL);
}


//
// 'pdfioDictSetArray()' - Set a key array in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetArray(pdfio_dict_t  *dict,	// I - Dictionary
                  const char    *key,	// I - Key
                  pdfio_array_t *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type        = PDFIO_VALTYPE_ARRAY;
  temp.value.array = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetBoolean()' - Set a key boolean in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetBoolean(pdfio_dict_t *dict,	// I - Dictionary
                    const char   *key,	// I - Key
                    bool         value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type          = PDFIO_VALTYPE_BOOLEAN;
  temp.value.boolean = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetDict()' - Set a key dictionary in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetDict(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key,	// I - Key
                 pdfio_dict_t *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type       = PDFIO_VALTYPE_DICT;
  temp.value.dict = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetName()' - Set a key name in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetName(pdfio_dict_t  *dict,	// I - Dictionary
                 const char    *key,	// I - Key
                 const char    *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type       = PDFIO_VALTYPE_NAME;
  temp.value.name = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetNull()' - Set a key null in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetNull(pdfio_dict_t *dict,	// I - Dictionary
		 const char   *key)	// I - Key
{
  _pdfio_value_t temp;			// New value


  temp.type = PDFIO_VALTYPE_NULL;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetNumber()' - Set a key number in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetNumber(pdfio_dict_t  *dict,	// I - Dictionary
                   const char    *key,	// I - Key
                   float         value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type         = PDFIO_VALTYPE_NUMBER;
  temp.value.number = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetObject()' - Set a key indirect object reference in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetObject(pdfio_dict_t *dict,	// I - Dictionary
                  const char    *key,	// I - Key
                  pdfio_obj_t   *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type      = PDFIO_VALTYPE_INDIRECT;
  temp.value.obj = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetRect()' - Set a key rectangle in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetRect(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key,	// I - Key
                 pdfio_rect_t *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type        = PDFIO_VALTYPE_ARRAY;
  temp.value.array = pdfioArrayCreate(dict->pdf);

  pdfioArrayAppendNumber(temp.value.array, value->x1);
  pdfioArrayAppendNumber(temp.value.array, value->y1);
  pdfioArrayAppendNumber(temp.value.array, value->x2);
  pdfioArrayAppendNumber(temp.value.array, value->y2);

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetString()' - Set a key literal string in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetString(pdfio_dict_t  *dict,	// I - Dictionary
                  const char     *key,	// I - Key
                  const char     *value)// I - Value
{
  _pdfio_value_t temp;			// New value


  temp.type         = PDFIO_VALTYPE_STRING;
  temp.value.string = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetStringf()' - Set a key formatted string in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetStringf(
    pdfio_dict_t  *dict,		// I - Dictionary
    const char    *key,			// I - Key
    const char    *format,		// I - `printf`-style format string
    ...)				// I - Additional arguments as needed
{
  char		buffer[8192];		// String buffer
  va_list	ap;			// Argument list


  va_start(ap, format);
  vsnprintf(buffer, sizeof(buffer), format, ap);
  va_end(ap);

  return (pdfioDictSetString(dict, key, buffer));
}


//
// '_pdfioDictSetValue()' - Set a key value in a dictionary.
//

bool					// O - `true` on success, `false` on failure
_pdfioDictSetValue(
    pdfio_dict_t   *dict,		// I - Dictionary
    const char     *key,		// I - Key
    _pdfio_value_t *value)		// I - Value
{
  _pdfio_pair_t	temp,			// Search key
		*pair;			// Current pair


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // See if the key is already set...
  if (dict->num_pairs > 0)
  {
    temp.key = key;

    if ((pair = (_pdfio_pair_t *)bsearch(&temp, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
    {
      // Yes, replace the value...
      pair->value = *value;
      return (true);
    }
  }

  // Nope, add a pair...
  if (dict->num_pairs >= dict->alloc_pairs)
  {
    // Expand the dictionary...
    _pdfio_pair_t *temp = (_pdfio_pair_t *)realloc(dict->pairs, (dict->alloc_pairs + 16) * sizeof(_pdfio_pair_t));

    if (!temp)
      return (false);

    dict->pairs       = temp;
    dict->alloc_pairs += 16;
  }

  pair = dict->pairs + dict->num_pairs;
  dict->num_pairs ++;

  pair->key   = key;
  pair->value = *value;

  // Re-sort the dictionary and return...
  if (dict->num_pairs > 1)
    qsort(dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs);

  return (true);
}


//
// 'compare_pairs()' - Compare the keys for two pairs.
//

static int				// O - Result of comparison
compare_pairs(_pdfio_pair_t *a,		// I - First pair
              _pdfio_pair_t *b)		// I - Second pair
{
  return (strcmp(a->key, b->key));
}
