//
// PDF value functions for pdfio.
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
// '_pdfioValueCopy()' - Copy a value to a PDF file.
//

_pdfio_value_t	*
_pdfioValueCopy(pdfio_file_t   *pdfdst,	// I - Destination PDF file
                _pdfio_value_t *vdst,	// I - Destination value
                pdfio_file_t   *pdfsrc,	// I - Source PDF file
                _pdfio_value_t *vsrc)	// I - Source value
{
  pdfio_obj_t	*obj;			// Object reference
#ifdef DEBUG
  static const char * const types[] =	// Type strings for debug
  {
    "PDFIO_VALTYPE_NONE",
    "PDFIO_VALTYPE_ARRAY",
    "PDFIO_VALTYPE_BINARY",
    "PDFIO_VALTYPE_BOOLEAN",
    "PDFIO_VALTYPE_DATE",
    "PDFIO_VALTYPE_DICT",
    "PDFIO_VALTYPE_INDIRECT",
    "PDFIO_VALTYPE_NAME",
    "PDFIO_VALTYPE_NULL",
    "PDFIO_VALTYPE_NUMBER",
    "PDFIO_VALTYPE_STRING"
  };
#endif // DEBUG


  PDFIO_DEBUG("_pdfioValueCopy(pdfdst=%p, vdst=%p, pdfsrc=%p, vsrc=%p(%s))\n", pdfdst, vdst, pdfsrc, vsrc, types[vsrc->type]);

  if (pdfdst == pdfsrc && vsrc->type != PDFIO_VALTYPE_BINARY)
  {
    // For the same document we can copy the values without any other effort
    // unless there is a binary (hex string) value...
    *vdst = *vsrc;
    return (vdst);
  }

  // Not the same document or a binary value, do a deep copy...
  switch (vsrc->type)
  {
    case PDFIO_VALTYPE_INDIRECT :
        if ((obj = _pdfioFileFindMappedObject(pdfdst, pdfsrc, vsrc->value.indirect.number)) == NULL)
        {
          obj = pdfioObjCopy(pdfdst, pdfioFileFindObject(pdfsrc, vsrc->value.indirect.number));
	}

        if (!obj)
          return (NULL);

	vdst->value.indirect.number     = obj->number;
	vdst->value.indirect.generation = obj->generation;
	break;

    default :
        return (NULL);

    case PDFIO_VALTYPE_ARRAY :
        vdst->value.array = pdfioArrayCopy(pdfdst, vsrc->value.array);
        break;

    case PDFIO_VALTYPE_BINARY :
        if ((vdst->value.binary.data = (unsigned char *)malloc(vsrc->value.binary.datalen)) == NULL)
        {
          _pdfioFileError(pdfdst, "Unable to allocate memory for a binary string - %s", strerror(errno));
          return (NULL);
        }

        vdst->value.binary.datalen = vsrc->value.binary.datalen;
        memcpy(vdst->value.binary.data, vsrc->value.binary.data, vdst->value.binary.datalen);
        break;

    case PDFIO_VALTYPE_BOOLEAN :
    case PDFIO_VALTYPE_DATE :
    case PDFIO_VALTYPE_NUMBER :
	*vdst = *vsrc;
        return (vdst);

    case PDFIO_VALTYPE_DICT :
        vdst->value.dict = pdfioDictCopy(pdfdst, vsrc->value.dict);
        break;

    case PDFIO_VALTYPE_NAME :
    case PDFIO_VALTYPE_STRING :
        vdst->value.name = pdfioStringCreate(pdfdst, vsrc->value.name);
        break;
  }

  vdst->type = vsrc->type;

  return (vdst);
}


//
// '_pdfioValueDebug()' - Print the contents of a value.
//

void
_pdfioValueDebug(_pdfio_value_t *v,	// I - Value
		 FILE           *fp)	// I - Output file
{
  switch (v->type)
  {
    case PDFIO_VALTYPE_ARRAY :
        _pdfioArrayDebug(v->value.array, fp);
	break;
    case PDFIO_VALTYPE_BINARY :
	{
	  size_t	i;		// Looping var
	  unsigned char	*ptr;		// Pointer into data

	  putc('<', fp);
	  for (i = v->value.binary.datalen, ptr = v->value.binary.data; i > 0; i --, ptr ++)
	    fprintf(fp, "%02X", *ptr);
	  putc('>', fp);
	}
	break;
    case PDFIO_VALTYPE_BOOLEAN :
	fputs(v->value.boolean ? " true" : " false", fp);
	break;
    case PDFIO_VALTYPE_DATE :
        // TODO: Implement date value support
        fputs("(D:YYYYMMDDhhmmssZ)", fp);
        break;
    case PDFIO_VALTYPE_DICT :
	fputs("<<", fp);
	_pdfioDictDebug(v->value.dict, fp);
	fputs(">>", fp);
	break;
    case PDFIO_VALTYPE_INDIRECT :
	fprintf(fp, " %lu %u R", (unsigned long)v->value.indirect.number, v->value.indirect.generation);
	break;
    case PDFIO_VALTYPE_NAME :
	fprintf(fp, "/%s", v->value.name);
	break;
    case PDFIO_VALTYPE_NULL :
	fputs(" null", fp);
	break;
    case PDFIO_VALTYPE_NUMBER :
	fprintf(fp, " %g", v->value.number);
	break;
    case PDFIO_VALTYPE_STRING :
	fprintf(fp, "(%s)", v->value.string);
	break;

    default :
        break;
  }
}


//
// '_pdfioValueDelete()' - Free the memory used by a value.
//

void
_pdfioValueDelete(_pdfio_value_t *v)	// I - Value
{
  if (v->type == PDFIO_VALTYPE_BINARY)
    free(v->value.binary.data);
}


//
// '_pdfioValueRead()' - Read a value from a file.
//

_pdfio_value_t *			// O - Value or `NULL` on error/EOF
_pdfioValueRead(pdfio_file_t   *pdf,	// I - PDF file
                _pdfio_token_t *tb,	// I - Token buffer/stack
                _pdfio_value_t *v)	// I - Value
{
  char		token[8192];		// Token buffer
#ifdef DEBUG
  static const char * const valtypes[] =
  {
    "<<none>>",				// No value, not set
    "array",				// Array
    "hex-string",			// Binary data
    "boolean",				// Boolean
    "date",				// Date/time
    "dict",				// Dictionary
    "indirect",				// Indirect object (N G obj)
    "name",				// Name
    "null",				// Null object
    "number",				// Number (integer or real)
    "string"				// String
  };
#endif // DEBUG


  PDFIO_DEBUG("_pdfioValueRead(pdf=%p, v=%p)\n", pdf, v);

  if (!_pdfioTokenGet(tb, token, sizeof(token)))
    return (NULL);

  if (!strcmp(token, "["))
  {
    // Start of array
    v->type = PDFIO_VALTYPE_ARRAY;
    if ((v->value.array = _pdfioArrayRead(pdf, tb)) == NULL)
      return (NULL);
  }
  else if (!strcmp(token, "<<"))
  {
    // Start of dictionary
    v->type = PDFIO_VALTYPE_DICT;
    if ((v->value.dict = _pdfioDictRead(pdf, tb)) == NULL)
      return (NULL);
  }
  else if (token[0] == '(')
  {
    // TODO: Add date value support
    // String
    v->type         = PDFIO_VALTYPE_STRING;
    v->value.string = pdfioStringCreate(pdf, token + 1);
  }
  else if (token[0] == '/')
  {
    // Name
    v->type       = PDFIO_VALTYPE_NAME;
    v->value.name = pdfioStringCreate(pdf, token + 1);
  }
  else if (token[0] == '<')
  {
    // Hex string
    const char		*tokptr;	// Pointer into token
    unsigned char	*dataptr;	// Pointer into data

    v->type                 = PDFIO_VALTYPE_BINARY;
    v->value.binary.datalen = strlen(token) / 2;
    if ((v->value.binary.data = (unsigned char *)malloc(v->value.binary.datalen)) == NULL)
    {
      _pdfioFileError(pdf, "Out of memory for hex string.");
      return (NULL);
    }

    // Convert hex to binary...
    tokptr  = token + 1;
    dataptr = v->value.binary.data;

    while (*tokptr)
    {
      int	d;			// Data value

      if (isdigit(*tokptr))
	d = (*tokptr++ - '0') << 4;
      else
	d = (tolower(*tokptr++) - 'a' + 10) << 4;

      if (*tokptr)
      {
	// PDF allows writers to drop a trailing 0...
	if (isdigit(*tokptr))
	  d |= *tokptr++ - '0';
	else
	  d |= tolower(*tokptr++) - 'a' + 10;
      }

      *dataptr++ = (unsigned char)d;
    }
  }
  else if (strchr("0123456789-+.", token[0]) != NULL)
  {
    // Number or indirect object reference
    if (isdigit(token[0]) && !strchr(token, '.'))
    {
      // Integer or object ref...
      char	token2[8192],		// Second token (generation number)
		token3[8192],		// Third token ("R")
		*tokptr;		// Pointer into token

      if (_pdfioTokenGet(tb, token2, sizeof(token2)))
      {
        // Got the second token, is it an integer?
        for (tokptr = token2; *tokptr; tokptr ++)
	{
	  if (!isdigit(*tokptr))
	    break;
	}

	if (*tokptr)
	{
	  // Not an object reference, push this token for later use...
	  _pdfioTokenPush(tb, token2);
	}
	else
	{
	  // A possible reference, get one more...
	  if (_pdfioTokenGet(tb, token3, sizeof(token3)))
	  {
	    if (!strcmp(token3, "R"))
	    {
	      // Reference!
	      v->type                      = PDFIO_VALTYPE_INDIRECT;
	      v->value.indirect.number     = (size_t)strtoimax(token, NULL, 10);
	      v->value.indirect.generation = (unsigned short)strtol(token2, NULL, 10);

              PDFIO_DEBUG("_pdfioValueRead: Returning indirect value %lu %u R.\n", (unsigned long)v->value.indirect.number, v->value.indirect.generation);

              return (v);
	    }
	    else
	    {
	      // Not a reference, push the tokens back...
	      _pdfioTokenPush(tb, token3);
	      _pdfioTokenPush(tb, token2);
	    }
	  }
	  else
	  {
	    // Not a reference...
	    _pdfioTokenPush(tb, token2);
	  }
	}
      }
    }

    // If we get here, we have a number...
    v->type         = PDFIO_VALTYPE_NUMBER;
    v->value.number = (float)strtod(token, NULL);
  }
  else if (!strcmp(token, "true") || !strcmp(token, "false"))
  {
    // Boolean value
    v->type          = PDFIO_VALTYPE_BOOLEAN;
    v->value.boolean = !strcmp(token, "true");
  }
  else if (!strcmp(token, "null"))
  {
    // null value
    v->type = PDFIO_VALTYPE_NULL;
  }
  else
  {
    _pdfioFileError(pdf, "Unexpected '%s' token seen.", token);
    return (NULL);
  }

  PDFIO_DEBUG("_pdfioValueRead: Returning %s value.\n", valtypes[v->type]);

  return (v);
}


//
// '_pdfioValueWrite()' - Write a value to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioValueWrite(pdfio_file_t   *pdf,	// I - PDF file
                 _pdfio_value_t *v,	// I - Value
                 off_t          *length)// O - Offset to /Length value, if any
{
  switch (v->type)
  {
    default :
        return (false);

    case PDFIO_VALTYPE_ARRAY :
        return (_pdfioArrayWrite(v->value.array));

    case PDFIO_VALTYPE_BINARY :
        {
          size_t	i;		// Looping var
          unsigned char	*dataptr;	// Pointer into data

          if (!_pdfioFilePuts(pdf, "<"))
            return (false);

          for (i = v->value.binary.datalen, dataptr = v->value.binary.data; i > 1; i -= 2, dataptr += 2)
          {
            if (!_pdfioFilePrintf(pdf, "%02X%02X", dataptr[0], dataptr[1]))
              return (false);
          }

          if (i > 0)
            return (_pdfioFilePrintf(pdf, "%02X>", dataptr[0]));
          else
            return (_pdfioFilePuts(pdf, ">"));
        }

    case PDFIO_VALTYPE_BOOLEAN :
        if (v->value.boolean)
          return (_pdfioFilePuts(pdf, " true"));
        else
          return (_pdfioFilePuts(pdf, " false"));

    case PDFIO_VALTYPE_DATE :
        {
          struct tm	date;		// Date values

          gmtime_r(&v->value.date, &date);
          return (_pdfioFilePrintf(pdf, "(D:%04d%02d%02d%02d%02d%02dZ)", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec));
        }

    case PDFIO_VALTYPE_DICT :
        return (_pdfioDictWrite(v->value.dict, length));

    case PDFIO_VALTYPE_INDIRECT :
        return (_pdfioFilePrintf(pdf, " %lu %u R", (unsigned long)v->value.indirect.number, v->value.indirect.generation));

    case PDFIO_VALTYPE_NAME :
        return (_pdfioFilePrintf(pdf, "/%s", v->value.name));

    case PDFIO_VALTYPE_NULL :
        return (_pdfioFilePuts(pdf, " null"));

    case PDFIO_VALTYPE_NUMBER :
        return (_pdfioFilePrintf(pdf, " %g", v->value.number));

    case PDFIO_VALTYPE_STRING :
        {
          const char *start,		// Start of fragment
		     *end;		// End of fragment

          if (!_pdfioFilePuts(pdf, "("))
            return (false);

          // Write a quoted string value...
          for (start = v->value.string; *start; start = end)
          {
            // Find the next character that needs to be quoted...
            for (end = start; *end; end ++)
            {
              if (*end == '\\' || *end == ')' || (*end & 255) < ' ')
                break;
            }

            if (end > start)
            {
              // Write unquoted (safe) characters...
	      if (!_pdfioFileWrite(pdf, start, (size_t)(end - start)))
		return (false);
	    }

            if (*end)
            {
              // Quote this character...
              bool success;		// Did the write work?

              if (*end == '\\' || *end == ')')
                success = _pdfioFilePrintf(pdf, "\\%c", *end);
              else
                success = _pdfioFilePrintf(pdf, "\\%03o", *end);

              if (!success)
                return (false);

              end ++;
            }
          }

          return (_pdfioFilePuts(pdf, ")"));
        }
  }

  return (false);
}
