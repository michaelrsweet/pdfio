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
// '()' - .
//

pdfio_dict_t	*pdfioDictCreate(pdfio_file_t *file)
{
}


//
// '()' - .
//

void	_pdfioDictDelete(pdfio_dict_t *dict)
{
}


//
// '()' - .
//

pdfio_array_t	*pdfioDictGetArray(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

bool		pdfioDictGetBoolean(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

pdfio_dict_t	*pdfioDictGetDict(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

const char	*pdfioDictGetName(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

float		pdfioDictGetNumber(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioDictGetObject(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

pdfio_rect_t	*pdfioDictGetRect(pdfio_dict_t *dict, const char *name, pdfio_rect_t *rect)
{
}


//
// '()' - .
//

const char	*pdfioDictGetString(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

pdfio_valtype_t	pdfioDictGetType(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

bool		pdfioDictSetArray(pdfio_dict_t *dict, const char *name, pdfio_array_t *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetBoolean(pdfio_dict_t *dict, const char *name, bool value)
{
}


//
// '()' - .
//

bool		pdfioDictSetDict(pdfio_dict_t *dict, const char *name, pdfio_dict_t *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetName(pdfio_dict_t *dict, const char *name, const char *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetNull(pdfio_dict_t *dict, const char *name)
{
}


//
// '()' - .
//

bool		pdfioDictSetNumber(pdfio_dict_t *dict, const char *name, float value)
{
}


//
// '()' - .
//

bool		pdfioDictSetObject(pdfio_dict_t *dict, const char *name, pdfio_obj_t *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetRect(pdfio_dict_t *dict, const char *name, pdfio_rect_t *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetString(pdfio_dict_t *dict, const char *name, const char *value)
{
}


//
// '()' - .
//

bool		pdfioDictSetStringf(pdfio_dict_t *dict, const char *name, const char *format, ...)
{
}
