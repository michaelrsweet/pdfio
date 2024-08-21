//
// PDF value functions for PDFio.
//
// Copyright © 2021-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"


//
// Local functions...
//

static time_t	get_date_time(const char *s);


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
        if ((obj = _pdfioFileFindMappedObj(pdfdst, pdfsrc, vsrc->value.indirect.number)) == NULL)
        {
          obj = pdfioObjCopy(pdfdst, pdfioFileFindObj(pdfsrc, vsrc->value.indirect.number));
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
// '_pdfioValueDecrypt()' - Decrypt a value.
//

bool					// O - `true` on success, `false` on error
_pdfioValueDecrypt(pdfio_file_t   *pdf,	// I - PDF file
                   pdfio_obj_t    *obj,	// I - Object
                   _pdfio_value_t *v,	// I - Value
                   size_t         depth)// I - Depth
{
  _pdfio_crypto_ctx_t	ctx;		// Decryption context
  _pdfio_crypto_cb_t	cb;		// Decryption callback
  size_t		ivlen;		// Number of initialization vector bytes
  uint8_t		temp[32768];	// Temporary buffer for decryption
  size_t		templen;	// Number of actual data bytes
  time_t		timeval;	// Date/time value


  if (depth > PDFIO_MAX_DEPTH)
  {
    _pdfioFileError(pdf, "Value too deep.");
    return (false);
  }

  switch (v->type)
  {
    default :
        // Do nothing
        break;

    case PDFIO_VALTYPE_ARRAY :
        return (_pdfioArrayDecrypt(pdf, obj, v->value.array, depth + 1));
        break;

    case PDFIO_VALTYPE_DICT :
        return (_pdfioDictDecrypt(pdf, obj, v->value.dict, depth + 1));
        break;

    case PDFIO_VALTYPE_BINARY :
	// Decrypt the binary string...
	if (v->value.binary.datalen > (sizeof(temp) - 32))
	{
	  _pdfioFileError(pdf, "Unable to read encrypted binary string - too long.");
	  return (false);
	}

	ivlen = v->value.binary.datalen;
	if ((cb = _pdfioCryptoMakeReader(pdf, obj, &ctx, v->value.binary.data, &ivlen)) == NULL)
	  return (false);

	templen = (cb)(&ctx, temp, v->value.binary.data + ivlen, v->value.binary.datalen - ivlen);

	// Copy the decrypted string back to the value and adjust the length...
	memcpy(v->value.binary.data, temp, templen);

	if (pdf->encryption >= PDFIO_ENCRYPTION_AES_128)
	  v->value.binary.datalen = templen - temp[templen - 1];
	else
	  v->value.binary.datalen = templen;
	break;

    case PDFIO_VALTYPE_STRING :
        // Decrypt regular string...
        templen = strlen(v->value.string);
	if (templen > (sizeof(temp) - 33))
	{
	  _pdfioFileError(pdf, "Unable to read encrypted string - too long.");
	  return (false);
	}

	ivlen = templen;
	if ((cb = _pdfioCryptoMakeReader(pdf, obj, &ctx, (uint8_t *)v->value.string, &ivlen)) == NULL)
	  return (false);

	templen = (cb)(&ctx, temp, (uint8_t *)v->value.string + ivlen, templen - ivlen);
	temp[templen] = '\0';

        if ((timeval = get_date_time((char *)temp)) != 0)
        {
          // Change the type to date...
          v->type       = PDFIO_VALTYPE_DATE;
          v->value.date = timeval;
        }
        else
        {
          // Copy the decrypted string back to the value...
	  v->value.string = pdfioStringCreate(pdf, (char *)temp);
	}
        break;
  }

  return (true);
}


//
// '_pdfioValueDebug()' - Print the contents of a value.
//

void
_pdfioValueDebug(_pdfio_value_t *v,	// I - Value
		 FILE           *fp)	// I - Output file
{
  if (!v)
    return;

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
        {
	  struct tm	dateval;	// Date value

#ifdef _WIN32
          gmtime_s(&dateval, &v->value.date);
#else
          gmtime_r(&v->value.date, &dateval);
#endif // _WIN32

          fprintf(fp, "(D:%04d%02d%02d%02d%02d%02dZ)", dateval.tm_year + 1900, dateval.tm_mon + 1, dateval.tm_mday, dateval.tm_hour, dateval.tm_min, dateval.tm_sec);
        }
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
                pdfio_obj_t    *obj,	// I - Object, if any
                _pdfio_token_t *tb,	// I - Token buffer/stack
                _pdfio_value_t *v,	// I - Value
                size_t         depth)	// I - Depth of value
{
  char		token[32768];		// Token buffer
  time_t	timeval;		// Date/time value
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


  PDFIO_DEBUG("_pdfioValueRead(pdf=%p, obj=%p, v=%p)\n", pdf, obj, v);

  if (!_pdfioTokenGet(tb, token, sizeof(token)))
    return (NULL);

  if (!strcmp(token, "["))
  {
    // Start of array
    if (depth >= PDFIO_MAX_DEPTH)
    {
      _pdfioFileError(pdf, "Too many nested arrays.");
      return (NULL);
    }

    v->type = PDFIO_VALTYPE_ARRAY;
    if ((v->value.array = _pdfioArrayRead(pdf, obj, tb, depth + 1)) == NULL)
      return (NULL);
  }
  else if (!strcmp(token, "<<"))
  {
    // Start of dictionary
    if (depth >= PDFIO_MAX_DEPTH)
    {
      _pdfioFileError(pdf, "Too many nested dictionaries.");
      return (NULL);
    }

    v->type = PDFIO_VALTYPE_DICT;
    if ((v->value.dict = _pdfioDictRead(pdf, obj, tb, depth + 1)) == NULL)
      return (NULL);
  }
  else if (!strncmp(token, "(D:", 3) && (timeval = get_date_time(token + 1)) != 0)
  {
    v->type       = PDFIO_VALTYPE_DATE;
    v->value.date = timeval;
  }
  else if (token[0] == '(')
  {
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
      unsigned char *tempptr;		// Pointer into buffer

#ifdef DEBUG
      PDFIO_DEBUG("_pdfioValueRead: %d bytes left in buffer: '", (int)(tb->bufend - tb->bufptr));
      for (tempptr = tb->bufptr; tempptr < tb->bufend; tempptr ++)
      {
	if (*tempptr < ' ' || *tempptr == 0x7f)
	  PDFIO_DEBUG("\\%03o", *tempptr);
	else
	  PDFIO_DEBUG("%c", *tempptr);
      }
      PDFIO_DEBUG("'.\n");
#endif // DEBUG

      if ((tb->bufend - tb->bufptr) < 10)
      {
        // Fill up buffer...
        ssize_t	bytes;			// Bytes peeked

        _pdfioTokenFlush(tb);

        if ((bytes = (tb->peek_cb)(tb->cb_data, tb->buffer, sizeof(tb->buffer))) > 0)
	  tb->bufend = tb->buffer + bytes;

#ifdef DEBUG
	PDFIO_DEBUG("_pdfioValueRead: %d bytes now in buffer: '", (int)(tb->bufend - tb->bufptr));
	for (tempptr = tb->bufptr; tempptr < tb->bufend; tempptr ++)
	{
	  if (*tempptr < ' ' || *tempptr == 0x7f)
	    PDFIO_DEBUG("\\%03o", *tempptr);
	  else
	    PDFIO_DEBUG("%c", *tempptr);
	}
	PDFIO_DEBUG("'.\n");
#endif // DEBUG
      }

      tempptr = tb->bufptr;

      while (tempptr < tb->bufend && isspace(*tempptr & 255))
        tempptr ++;			// Skip whitespace as needed...

      if (tempptr < tb->bufend && isdigit(*tempptr & 255))
      {
        // Integer...
        long generation = 0;		// Generation number

        while (tempptr < tb->bufend && isdigit(*tempptr & 255))
        {
          generation = generation * 10 + *tempptr - '0';
          tempptr ++;
        }

	while (tempptr < tb->bufend && isspace(*tempptr & 255))
	  tempptr ++;			// Skip whitespace

	if (tempptr < tb->bufend && *tempptr == 'R')
	{
	  // Reference!
	  PDFIO_DEBUG("_pdfioValueRead: Consuming %d bytes.\n", (int)(tempptr - tb->bufptr + 1));
	  tb->bufptr = tempptr + 1;

#ifdef DEBUG
	  PDFIO_DEBUG("_pdfioValueRead: Next bytes are '");
	  for (tempptr = tb->bufptr; tempptr < tb->bufend; tempptr ++)
	  {
	    if (*tempptr < ' ' || *tempptr == 0x7f)
	      PDFIO_DEBUG("\\%03o", *tempptr);
	    else
	      PDFIO_DEBUG("%c", *tempptr);
	  }
	  PDFIO_DEBUG("'.\n");
#endif // DEBUG

	  v->type                      = PDFIO_VALTYPE_INDIRECT;
	  v->value.indirect.number     = (size_t)strtoimax(token, NULL, 10);
	  v->value.indirect.generation = (unsigned short)generation;

	  PDFIO_DEBUG("_pdfioValueRead: Returning indirect value %lu %u R.\n", (unsigned long)v->value.indirect.number, v->value.indirect.generation);

	  return (v);
	}
      }
    }

    // If we get here, we have a number...
    v->type         = PDFIO_VALTYPE_NUMBER;
    v->value.number = _pdfio_strtod(pdf, token);
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
                 pdfio_obj_t    *obj,	// I - Object, if any
                 _pdfio_value_t *v,	// I - Value
                 off_t          *length)// O - Offset to /Length value, if any
{
  switch (v->type)
  {
    default :
        return (false);

    case PDFIO_VALTYPE_ARRAY :
        return (_pdfioArrayWrite(v->value.array, obj));

    case PDFIO_VALTYPE_BINARY :
        {
          size_t	databytes;	// Bytes to write
          uint8_t	temp[32768],	// Temporary buffer for encryption
			*dataptr;	// Pointer into data

          if (obj && pdf->encryption)
          {
	    // Write encrypted string...
	    _pdfio_crypto_ctx_t ctx;	// Encryption context
	    _pdfio_crypto_cb_t cb;	// Encryption callback
	    size_t	ivlen;		// Number of initialization vector bytes

            if (v->value.binary.datalen > (sizeof(temp) - 32))
            {
	      _pdfioFileError(pdf, "Unable to write encrypted binary string - too long.");
	      return (false);
            }

	    cb        = _pdfioCryptoMakeWriter(pdf, obj, &ctx, temp, &ivlen);
	    databytes = (cb)(&ctx, temp + ivlen, v->value.binary.data, v->value.binary.datalen) + ivlen;
	    dataptr   = temp;
          }
          else
          {
            dataptr   = v->value.binary.data;
            databytes = v->value.binary.datalen;
          }

          if (!_pdfioFilePuts(pdf, "<"))
            return (false);

          for (; databytes > 1; databytes -= 2, dataptr += 2)
          {
            if (!_pdfioFilePrintf(pdf, "%02X%02X", dataptr[0], dataptr[1]))
              return (false);
          }

          if (databytes > 0)
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
          char		datestr[32];	// Formatted date value

#ifdef _WIN32
          gmtime_s(&date, &v->value.date);
#else
	  gmtime_r(&v->value.date, &date);
#endif // _WIN32

	  snprintf(datestr, sizeof(datestr), "D:%04d%02d%02d%02d%02d%02dZ", date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, date.tm_hour, date.tm_min, date.tm_sec);

	  if (obj && pdf->encryption)
	  {
	    // Write encrypted string...
	    uint8_t	temp[32768],	// Encrypted bytes
			*tempptr;	// Pointer into encrypted bytes
	    _pdfio_crypto_ctx_t ctx;	// Encryption context
	    _pdfio_crypto_cb_t cb;	// Encryption callback
	    size_t	len = strlen(datestr),
					  // Length of value
			ivlen,		// Number of initialization vector bytes
			tempbytes;	// Number of output bytes

	    cb        = _pdfioCryptoMakeWriter(pdf, obj, &ctx, temp, &ivlen);
	    tempbytes = (cb)(&ctx, temp + ivlen, (const uint8_t *)datestr, len) + ivlen;

	    if (!_pdfioFilePuts(pdf, "<"))
	      return (false);

	    for (tempptr = temp; tempbytes > 1; tempbytes -= 2, tempptr += 2)
	    {
	      if (!_pdfioFilePrintf(pdf, "%02X%02X", tempptr[0], tempptr[1]))
		return (false);
	    }

            if (tempbytes > 0)
              return (_pdfioFilePrintf(pdf, "%02X>", *tempptr));
            else
	      return (_pdfioFilePuts(pdf, ">"));
	  }
	  else
	  {
	    return (_pdfioFilePrintf(pdf, "(%s)", datestr));
	  }
        }

    case PDFIO_VALTYPE_DICT :
        return (_pdfioDictWrite(v->value.dict, obj, length));

    case PDFIO_VALTYPE_INDIRECT :
        return (_pdfioFilePrintf(pdf, " %lu %u R", (unsigned long)v->value.indirect.number, v->value.indirect.generation));

    case PDFIO_VALTYPE_NAME :
        return (_pdfioFilePrintf(pdf, "/%s", v->value.name));

    case PDFIO_VALTYPE_NULL :
        return (_pdfioFilePuts(pdf, " null"));

    case PDFIO_VALTYPE_NUMBER :
        return (_pdfioFilePrintf(pdf, " %g", v->value.number));

    case PDFIO_VALTYPE_STRING :
        if (obj && pdf->encryption)
        {
          // Write encrypted string...
          uint8_t	temp[32768],	// Encrypted bytes
			*tempptr;	// Pointer into encrypted bytes
          _pdfio_crypto_ctx_t ctx;	// Encryption context
          _pdfio_crypto_cb_t cb;	// Encryption callback
          size_t	len = strlen(v->value.string),
					// Length of value
			ivlen,		// Number of initialization vector bytes
			tempbytes;	// Number of output bytes

          if (len > (sizeof(temp) - 32))
          {
            _pdfioFileError(pdf, "Unable to write encrypted string - too long.");
            return (false);
          }

          cb        = _pdfioCryptoMakeWriter(pdf, obj, &ctx, temp, &ivlen);
          tempbytes = (cb)(&ctx, temp + ivlen, (const uint8_t *)v->value.string, len) + ivlen;

          if (!_pdfioFilePuts(pdf, "<"))
            return (false);

          for (tempptr = temp; tempbytes > 1; tempbytes -= 2, tempptr += 2)
          {
            if (!_pdfioFilePrintf(pdf, "%02X%02X", tempptr[0], tempptr[1]))
              return (false);
          }

          if (tempbytes > 0)
            return (_pdfioFilePrintf(pdf, "%02X>", *tempptr));
          else
	    return (_pdfioFilePuts(pdf, ">"));
        }
        else
        {
          // Write unencrypted string...
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


//
// 'get_date_time()' - Convert PDF date/time value to time_t.
//

static time_t				// O - Time in seconds
get_date_time(const char *s)		// I - PDF date/time value
{
  int		i;			// Looping var
  struct tm	dateval;		// Date value
  int		offset;			// Date offset


  // Possible date value of the form:
  //
  //   (D:YYYYMMDDhhmmssZ)
  //   (D:YYYYMMDDhhmmss+HH'mm)
  //   (D:YYYYMMDDhhmmss-HH'mm)
  //

  for (i = 2; i < 16; i ++)
  {
    if (!isdigit(s[i] & 255) || !s[i])
      break;
  }

  if (i >= 16)
  {
    if (s[i] == 'Z')
    {
      i ++;
    }
    else if (s[i] == '-' || s[i] == '+')
    {
      if (isdigit(s[i + 1] & 255) && isdigit(s[i + 2] & 255) && s[i + 3] == '\'' && isdigit(s[i + 4] & 255) && isdigit(s[i + 5] & 255))
      {
	i += 6;
	if (s[i] == '\'')
	  i ++;
      }
    }
  }

  if (s[i])
  {
    // Just a string...
    return (0);
  }

  // Date value...
  memset(&dateval, 0, sizeof(dateval));

  dateval.tm_year = (s[2] - '0') * 1000 + (s[3] - '0') * 100 + (s[4] - '0') * 10 + s[5] - '0' - 1900;
  dateval.tm_mon  = (s[6] - '0') * 10 + s[7] - '0' - 1;
  dateval.tm_mday = (s[8] - '0') * 10 + s[9] - '0';
  dateval.tm_hour = (s[10] - '0') * 10 + s[11] - '0';
  dateval.tm_min  = (s[12] - '0') * 10 + s[13] - '0';
  dateval.tm_sec  = (s[14] - '0') * 10 + s[15] - '0';

  if (s[16] == 'Z')
  {
    offset = 0;
  }
  else
  {
    offset = (s[17] - '0') * 600 + (s[18] - '0') * 60 + (s[19] - '0') * 10 + s[20] - '0';
    if (s[16] == '-')
      offset = -offset;
  }

  return (mktime(&dateval) + offset);
}
