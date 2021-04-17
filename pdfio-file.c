//
// PDF file functions for pdfio.
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

bool		pdfioFileClose(pdfio_file_t *pdf)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioFileCreateObject(pdfio_file_t *file, pdfio_dict_t *dict)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioFileCreatePage(pdfio_t *pdf, pdfio_dict_t *dict)
{
}


//
// '()' - .
//

void	_pdfioFileDelete(pdfio_file_t *file)
{
}


//
// '()' - .
//

const char	*pdfioFileGetName(pdfio_file_t *pdf)
{
}


//
// '()' - .
//

int		pdfioFileGetNumObjects(pdfio_file_t *pdf)
{
}


//
// '()' - .
//

int		pdfioFileGetNumPages(pdfio_file_t *pdf)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioFileGetObject(pdfio_file_t *pdf, int number)
{
}


//
// '()' - .
//

pdfio_obj_t	*pdfioFileGetPage(pdfio_file_t *pdf, int number)
{
}


//
// '()' - .
//

pdfio_file_t	*pdfioFileOpen(const char *filename, const char *mode, pdfio_error_cb_t error_cb, void *error_data)
{
}


