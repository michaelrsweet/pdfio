//
// PDF dictionary functions for PDFio.
//
// Copyright © 2021-2023 by Michael R Sweet.
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
// '_pdfioDictClear()' - Remove a key/value pair from a dictionary.
//

void
_pdfioDictClear(pdfio_dict_t *dict,	// I - Dictionary
                const char   *key)	// I - Key
{
  size_t	idx;			// Index into pairs
  _pdfio_pair_t	*pair,			// Current pair
		pkey;			// Search key


  PDFIO_DEBUG("_pdfioDictClear(dict=%p, key=\"%s\")\n", dict, key);

  // See if the key is already set...
  if (dict->num_pairs > 0)
  {
    pkey.key = key;

    if ((pair = (_pdfio_pair_t *)bsearch(&pkey, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
    {
      // Yes, remove it...
      if (pair->value.type == PDFIO_VALTYPE_BINARY)
        free(pair->value.value.binary.data);

      idx = (size_t)(pair - dict->pairs);
      dict->num_pairs --;

      if (idx < dict->num_pairs)
        memmove(pair, pair + 1, (dict->num_pairs - idx) * sizeof(_pdfio_pair_t));
    }
  }
}


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


  PDFIO_DEBUG("pdfioDictCopy(pdf=%p, dict=%p(%p))\n", pdf, dict, dict ? dict->pdf : NULL);

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
    if (!strcmp(p->key, "Length") && p->value.type == PDFIO_VALTYPE_INDIRECT && dict->pdf != pdf)
    {
      // Don't use indirect stream lengths for copied objects...
      pdfio_obj_t *lenobj = pdfioFileFindObj(dict->pdf, p->value.value.indirect.number);
					// Length object

      v.type = PDFIO_VALTYPE_NUMBER;
      if (lenobj)
      {
        if (lenobj->value.type == PDFIO_VALTYPE_NONE)
          _pdfioObjLoad(lenobj);

	v.value.number = lenobj->value.value.number;
      }
      else
        v.value.number = 0.0;
    }
    else if (!_pdfioValueCopy(pdf, &v, dict->pdf, &p->value))
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
// '_pdfioDictDebug()' - Dump a dictionary to stderr.
//

void
_pdfioDictDebug(pdfio_dict_t *dict,	// I - Dictionary
                FILE         *fp)	// I - Output file
{
  size_t	i;			// Looping var
  _pdfio_pair_t	*pair;			// Current pair


  for (i = dict->num_pairs, pair = dict->pairs; i > 0; i --, pair ++)
  {
    fprintf(fp, "/%s", pair->key);
    _pdfioValueDebug(&pair->value, fp);
  }
}


//
// '_pdfioDictDelete()' - Free the memory used by a dictionary.
//

void
_pdfioDictDelete(pdfio_dict_t *dict)	// I - Dictionary
{
  if (dict)
  {
    size_t	i;			// Looping var
    _pdfio_pair_t *pair;		// Current pair

    for (i = dict->num_pairs, pair = dict->pairs; i > 0; i --, pair ++)
    {
      if (pair->value.type == PDFIO_VALTYPE_BINARY)
        free(pair->value.value.binary.data);
    }

    free(dict->pairs);
  }

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
  else if (value && value->type == PDFIO_VALTYPE_STRING)
  {
    *length = strlen(value->value.string);
    return ((unsigned char *)value->value.string);
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
// 'pdfioDictGetDate()' - Get a date value from a dictionary.
//

time_t					// O - Value
pdfioDictGetDate(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);


  if (value && value->type == PDFIO_VALTYPE_DATE)
    return (value->value.date);
  else
    return (0);
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

double					// O - Value
pdfioDictGetNumber(pdfio_dict_t *dict,	// I - Dictionary
                   const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);


  if (value && value->type == PDFIO_VALTYPE_NUMBER)
    return (value->value.number);
  else
    return (0.0);
}


//
// 'pdfioDictGetObj()' - Get a key indirect object value from a dictionary.
//

pdfio_obj_t *				// O - Value
pdfioDictGetObj(pdfio_dict_t *dict,	// I - Dictionary
                const char   *key)	// I - Key
{
  _pdfio_value_t *value = _pdfioDictGetValue(dict, key);


  if (value && value->type == PDFIO_VALTYPE_INDIRECT)
    return (pdfioFileFindObj(dict->pdf, value->value.indirect.number));
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


  PDFIO_DEBUG("_pdfioDictGetValue(dict=%p, key=\"%s\")\n", dict, key);

  if (!dict || !dict->num_pairs || !key)
  {
    PDFIO_DEBUG("_pdfioDictGetValue: Returning NULL.\n");
    return (NULL);
  }

  temp.key = key;

  if ((match = bsearch(&temp, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
  {
    PDFIO_DEBUG("_pdfioDictGetValue: Match, returning ");
    PDFIO_DEBUG_VALUE(&(match->value));
    PDFIO_DEBUG(".\n");
    return (&(match->value));
  }
  else
  {
    PDFIO_DEBUG("_pdfioDictGetValue: No match, returning NULL.\n");
    return (NULL);
  }
}


//
// 'pdfioDictIterateKeys()' - Iterate the keys in a dictionary.
//
// This function iterates the keys in a dictionary, calling the supplied
// function "cb":
//
// ```
// bool
// my_dict_cb(pdfio_dict_t *dict, const char *key, void *cb_data)
// {
// ... "key" contains the dictionary key ...
// ... return true to continue or false to stop ...
// }
// ```
//
// The iteration continues as long as the callback returns `true` or all keys
// have been iterated.
//

void
pdfioDictIterateKeys(
    pdfio_dict_t    *dict,		// I - Dictionary
    pdfio_dict_cb_t cb,			// I - Callback function
    void            *cb_data)		// I - Callback data
{
  size_t	i;			// Looping var
  _pdfio_pair_t	*pair;			// Current pair


  // Range check input...
  if (!dict || !cb)
    return;

  for (i = dict->num_pairs, pair = dict->pairs; i > 0; i --, pair ++)
  {
    if (!(cb)(dict, pair->key, cb_data))
      break;
  }
}


//
// '_pdfioDictRead()' - Read a dictionary from a PDF file.
//
// At this point we've seen the initial "<<"...
//

pdfio_dict_t *				// O - New dictionary
_pdfioDictRead(pdfio_file_t   *pdf,	// I - PDF file
               pdfio_obj_t    *obj,	// I - Object, if any
               _pdfio_token_t *tb,	// I - Token buffer/stack
               size_t         depth)	// I - Depth of dictionary
{
  pdfio_dict_t		*dict;		// New dictionary
  char			key[256];	// Dictionary key
  _pdfio_value_t	value;		// Dictionary value


  PDFIO_DEBUG("_pdfioDictRead(pdf=%p)\n", pdf);

  // Create a dictionary and start reading...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

  while (_pdfioTokenGet(tb, key, sizeof(key)))
  {
    // Get the next key or end-of-dictionary...
    if (!strcmp(key, ">>"))
    {
      // End of dictionary...
      return (dict);
    }
    else if (key[0] != '/')
    {
      _pdfioFileError(pdf, "Invalid dictionary contents.");
      break;
    }
    else if (_pdfioDictGetValue(dict, key + 1))
    {
      _pdfioFileError(pdf, "Duplicate dictionary key '%s'.", key + 1);
      return (NULL);
    }

    // Then get the next value...
    PDFIO_DEBUG("_pdfioDictRead: Reading value for '%s'.\n", key + 1);

    if (!_pdfioValueRead(pdf, obj, tb, &value, depth))
    {
      _pdfioFileError(pdf, "Missing value for dictionary key.");
      break;
    }

    if (!_pdfioDictSetValue(dict, pdfioStringCreate(pdf, key + 1), &value))
      break;

//    PDFIO_DEBUG("_pdfioDictRead: Set %s.\n", key);
  }

  // Dictionary is invalid - pdfioFileClose will free the memory, return NULL
  // to indicate an error...
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
    pdfio_dict_t        *dict,		// I - Dictionary
    const char          *key,		// I - Key
    const unsigned char *value,		// I - Value
    size_t              valuelen)	// I - Length of value
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
// 'pdfioDictSetDate()' - Set a date value in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetDate(pdfio_dict_t *dict,	// I - Dictionary
                 const char   *key,	// I - Key
                 time_t       value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  // Range check input...
  if (!dict || !key)
    return (false);

  // Set the key/value pair...
  temp.type       = PDFIO_VALTYPE_DATE;
  temp.value.date = value;

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
                   double        value)	// I - Value
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
// 'pdfioDictSetObj()' - Set a key indirect object reference in a dictionary.
//

bool					// O - `true` on success, `false` on failure
pdfioDictSetObj(pdfio_dict_t *dict,	// I - Dictionary
                const char    *key,	// I - Key
                pdfio_obj_t   *value)	// I - Value
{
  _pdfio_value_t temp;			// New value


  // Range check input...
  if (!dict || !key || !value)
    return (false);

  // Set the key/value pair...
  temp.type                      = PDFIO_VALTYPE_INDIRECT;
  temp.value.indirect.number     = value->number;
  temp.value.indirect.generation = value->generation;

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


  PDFIO_DEBUG("_pdfioDictSetValue(dict=%p, key=\"%s\", value=%p)\n", dict, key, (void *)value);

  // See if the key is already set...
  if (dict->num_pairs > 0)
  {
    _pdfio_pair_t	pkey;		// Search key

    pkey.key = key;

    if ((pair = (_pdfio_pair_t *)bsearch(&pkey, dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs)) != NULL)
    {
      // Yes, replace the value...
      PDFIO_DEBUG("_pdfioDictSetValue: Replacing existing value.\n");
      if (pair->value.type == PDFIO_VALTYPE_BINARY)
        free(pair->value.value.binary.data);
      pair->value = *value;
      return (true);
    }
  }

  // Nope, add a pair...
  if (dict->num_pairs >= dict->alloc_pairs)
  {
    // Expand the dictionary...
    _pdfio_pair_t *temp = (_pdfio_pair_t *)realloc(dict->pairs, (dict->alloc_pairs + 8) * sizeof(_pdfio_pair_t));

    if (!temp)
    {
      PDFIO_DEBUG("_pdfioDictSetValue: Out of memory.\n");
      return (false);
    }

    dict->pairs       = temp;
    dict->alloc_pairs += 8;
  }

  pair = dict->pairs + dict->num_pairs;
  dict->num_pairs ++;

  pair->key   = key;
  pair->value = *value;

  // Re-sort the dictionary and return...
  if (dict->num_pairs > 1 && compare_pairs(pair - 1, pair) > 0)
    qsort(dict->pairs, dict->num_pairs, sizeof(_pdfio_pair_t), (int (*)(const void *, const void *))compare_pairs);

#ifdef DEBUG
  PDFIO_DEBUG("_pdfioDictSetValue(%p): %lu pairs\n", (void *)dict, (unsigned long)dict->num_pairs);
//  PDFIO_DEBUG("_pdfioDictSetValue(%p): ", (void *)dict);
//  PDFIO_DEBUG_DICT(dict);
//  PDFIO_DEBUG("\n");
#endif // DEBUG

  return (true);
}


//
// '_pdfioDictWrite()' - Write a dictionary to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioDictWrite(pdfio_dict_t *dict,	// I - Dictionary
		pdfio_obj_t  *obj,	// I - Object, if any
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

    if (length && !strcmp(pair->key, "Length") && pair->value.type == PDFIO_VALTYPE_NUMBER && pair->value.value.number <= 0.0)
    {
      // Writing an object dictionary with an undefined length
      *length = _pdfioFileTell(pdf) + 1;
      if (!_pdfioFilePuts(pdf, " 9999999999"))
        return (false);
    }
    else if (!_pdfioValueWrite(pdf, obj, &pair->value, NULL))
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
