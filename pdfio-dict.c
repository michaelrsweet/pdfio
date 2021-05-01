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
// Local functions...
//

static int	compare_pairs(_pdfio_pair_t *a, _pdfio_pair_t *b);


//
// 'pdfioDictCopy()' - Copy a dictionary to a PDF file.
//

pdfio_dict_t *				// O - New dictionary
pdfioDictCopy(pdfio_file_t *pdf,	// I - PDF file
              pdfio_dict_t *dict)	// I - Original dictionary
{
  pdfio_dict_t		*ndict;		// New dictionary
  size_t		i;		// Looping var
  _pdfio_pair_t		*p;		// Current source pair
  const char		*key;		// Current destination key
  _pdfio_value_t	v;		// Current destination value


  // Create the new dictionary...
  if ((ndict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  // Pre-allocate the pairs array to make this a little faster...
  if ((ndict->pairs = (_pdfio_pair_t *)malloc(dict->num_pairs * sizeof(_pdfio_pair_t))) == NULL)
    return (NULL);			// Let pdfioFileClose do the cleanup...

  ndict->alloc_pairs = dict->num_pairs;

  // Copy and add each of the source dictionary's key/value pairs...
  for (i = dict->num_pairs, p = dict->pairs; i > 0; i --, p ++)
  {
    if (!_pdfioValueCopy(pdf, &v, dict->pdf, &p->value))
      return (NULL);			// Let pdfioFileClose do the cleanup...

    if (_pdfioStringIsAllocated(dict->pdf, p->key))
      key = pdfioStringCreate(pdf, p->key);
    else
      key = p->key;

    if (!key)
      return (NULL);			// Let pdfioFileClose do the cleanup...

    // Cannot fail since we already allocated space for the pairs...
    _pdfioDictSetValue(ndict, key, &v);
  }

  // Successfully copied the dictionary, so return it...
  return (ndict);
}


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
// 'pdfioDictGetBinary()' - Get a key binary string value from a dictionary.
//

unsigned char *				// O - Value
pdfioDictGetBinary(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key,	// I - Key
                   size_t       *length)// O - Length of value
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);

  if (!length)
    return (NULL);

  if (value && value->type == PDFIO_VALTYPE_BINARY)
  {
    *length = value->value.binary.datalen;
    return (value->value.binary.data);
  }
  else
  {
    *length = 0;
    return (NULL);
  }
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
// '_pdfioDictRead()' - Read a dictionary from a PDF file.
//
// At this point we've seen the initial "<<"...
//

pdfio_dict_t *				// O - New dictionary
_pdfioDictRead(pdfio_file_t *pdf)	// I - PDF file
{
  pdfio_dict_t	*dict;			// New dictionary
  char		token[8192],		// Token buffer
		key[256];		// Dictionary key
  _pdfio_value_t value;			// Dictionary value


  (void)pdf;
  (void)dict;
  (void)token;
  (void)key;
  (void)value;

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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
  temp.type        = PDFIO_VALTYPE_ARRAY;
  temp.value.array = value;

  return (_pdfioDictSetValue(dict, key, &temp));
}


//
// 'pdfioDictSetBinary()' - Set a key binary string in a dictionary.
//


bool					// O - `true` on success, `false` on failure
pdfioDictSetBinary(
    pdfio_dict_t  *dict,		// I - Dictionary
    const char    *key,			// I - Key
    unsigned char *value,		// I - Value
    size_t        valuelen)		// I - Length of value
{
  _pdfio_value_t temp;			// New value


  // Range check input...
  if (!dict || !key || !value || !valuelen)
    return (false);

  // Set the key/value pair...
  temp.type                 = PDFIO_VALTYPE_BINARY;
  temp.value.binary.datalen = valuelen;

  if ((temp.value.binary.data = (unsigned char *)malloc(valuelen)) == NULL)
    return (false);

  memcpy(temp.value.binary.data, value, valuelen);

  if (!_pdfioDictSetValue(dict, key, &temp))
  {
    free(temp.value.binary.data);
    return (false);
  }

  return (true);
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


  // Range check input...
  if (!dict || !key)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
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


  // Range check input...
  if (!dict || !key || !format)
    return (false);

  // Set the key/value pair...
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
  _pdfio_pair_t	*pair;			// Current pair


  // See if the key is already set...
  if (dict->num_pairs > 0)
  {
    _pdfio_pair_t	pkey;		// Search key

    pkey.key = key;

    if ((pair = (_pdfio_pair_t *)bsearch(&pkey, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
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
// '_pdfioDictWrite()' - Write a dictionary to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioDictWrite(pdfio_dict_t *dict,	// I - Dictionary
                off_t        *length)	// I - Offset to length value
{
  pdfio_file_t	*pdf = dict->pdf;	// PDF file
  size_t	i;			// Looping var
  _pdfio_pair_t	*pair;			// Current key/value pair


  if (length)
    *length = 0;

  // Dictionaries are bounded by "<<" and ">>"...
  if (!_pdfioFilePuts(pdf, "<<"))
    return (false);

  // Write all of the key/value pairs...
  for (i = dict->num_pairs, pair = dict->pairs; i > 0; i --, pair ++)
  {
    if (!_pdfioFilePrintf(pdf, "/%s", pair->key))
      return (false);

    if (length && !strcmp(pair->key, "Length"))
    {
      // Writing an object dictionary with an undefined length
      *length = _pdfioFileTell(pdf);
      if (!_pdfioFilePuts(pdf, " 999999999"))
        return (false);
    }
    else if (!_pdfioValueWrite(pdf, &pair->value))
      return (false);
  }

  // Close it up...
  return (_pdfioFilePuts(pdf, ">>"));
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
