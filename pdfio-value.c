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
  // TODO: Implement me
  (void)pdfdst;
  (void)vdst;
  (void)pdfsrc;
  (void)vsrc;

  return (NULL);
}


//
// '_pdfioValueDelete()' - Free the memory used by a value.
//

void
_pdfioValueDelete(_pdfio_value_t *v)	// I - Value
{
  // TODO: Implement me
  (void)v;
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
        // TODO: Implement binary value support
        return (false);
    case PDFIO_VALTYPE_BOOLEAN :
        if (v->value.boolean)
          return (_pdfioFilePuts(pdf, " true"));
        else
          return (_pdfioFilePuts(pdf, " false"));

    case PDFIO_VALTYPE_DATE :
        // TODO: Implement date value support
        break;

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
