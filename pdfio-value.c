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
        // Object references don't copy to other documents
        _pdfioFileError(pdfdst, "Unable to copy indirect object reference between PDF files.");
        return (NULL);

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
// '_pdfioValueDelete()' - Free the memory used by a value.
//

void
_pdfioValueDelete(_pdfio_value_t *v)	// I - Value
{
  if (v->type == PDFIO_VALTYPE_BINARY)
    free(v->value.binary.data);
}


//
// '_pdfioValueWrite()' - Write a value to a PDF file.
//

bool					// O - `true` on success, `false` on failure
_pdfioValueWrite(pdfio_file_t   *pdf,	// I - PDF file
                 _pdfio_value_t *v)	// I - Value
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
        return (_pdfioDictWrite(v->value.dict, NULL));

    case PDFIO_VALTYPE_INDIRECT :
        return (_pdfioFilePrintf(pdf, " %lu %lu obj", (unsigned long)v->value.obj->number, (unsigned long)v->value.obj->generation));

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
