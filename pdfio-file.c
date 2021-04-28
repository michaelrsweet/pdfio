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
#ifndef O_BINARY
#  define O_BINARY 0
#endif // !O_BINARY


//
// Local functions...
//

static bool	write_trailer(pdfio_file_t *pdf);


//
// 'pdfioFileClose()' - Close a PDF file and free all memory used for it.
//

bool					// O - `true` on success and `false` on failure
pdfioFileClose(pdfio_file_t *pdf)	// I - PDF file
{
  bool		ret = true;		// Return value
  size_t	i;			// Looping var


  // Range check input
  if (!pdf)
    return (false);

  // Close the file itself...
  if (pdf->mode == _PDFIO_MODE_WRITE)
    ret = write_trailer(pdf);

  if (close(pdf->fd) < 0)
    ret = false;

  // Free all data...
  free(pdf->filename);
  free(pdf->version);

  for (i = 0; i < pdf->num_arrays; i ++)
    _pdfioArrayDelete(pdf->arrays[i]);
  free(pdf->arrays);

  for (i = 0; i < pdf->num_dicts; i ++)
    _pdfioDictDelete(pdf->dicts[i]);
  free(pdf->dicts);

  for (i = 0; i < pdf->num_objs; i ++)
    _pdfioObjDelete(pdf->objs[i]);
  free(pdf->objs);

  free(pdf->pages);

  for (i = 0; i < pdf->num_strings; i ++)
    free(pdf->strings[i]);
  free(pdf->strings);

  free(pdf);

  return (ret);
}


//
// 'pdfioFileCreate()' - Create a PDF file.
//

pdfio_file_t *				// O - PDF file or `NULL` on error
pdfioFileCreate(
    const char       *filename,		// I - Filename
    const char       *version,		// I - PDF version number or `NULL` for default (2.0)
    pdfio_error_cb_t error_cb,		// I - Error callback or `NULL` for default
    void             *error_data)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file


  // Range check input...
  if (!filename)
    return (NULL);

  if (!version)
    version = "2.0";

  if (!error_cb)
  {
    error_cb   = _pdfioFileDefaultError;
    error_data = NULL;
  }

  // Allocate a PDF file structure...
  if ((pdf = (pdfio_file_t *)calloc(1, sizeof(pdfio_file_t))) == NULL)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)filename;
    snprintf(message, sizeof(message), "Unable to allocate memory for PDF file - %s", strerror(errno));
    (error_cb)(&temp, message, error_data);
    return (NULL);
  }

  pdf->filename   = strdup(filename);
  pdf->version    = strdup(version);
  pdf->mode       = _PDFIO_MODE_WRITE;
  pdf->error_cb   = error_cb;
  pdf->error_data = error_data;

  // Create the file...
  if ((pdf->fd = open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0666)) < 0)
  {
    _pdfioFileError(pdf, "Unable to create file - %s", strerror(errno));
    free(pdf->filename);
    free(pdf->version);
    free(pdf);
    return (NULL);
  }

  // Write a standard PDF header...
  if (!_pdfioFilePrintf(pdf, "%%PDF-%s\n%%\342\343\317\323\n", version))
  {
    pdfioFileClose(pdf);
    unlink(filename);
    return (NULL);
  }

  return (pdf);
}


//
// 'pdfioFileCreateObject()' - Create a new object in a PDF file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateObject(
    pdfio_file_t *pdf,			// I - PDF file
    pdfio_dict_t *dict)			// I - Object dictionary
{
  pdfio_obj_t	*obj;			// New object


  // Range check input...
  if (!pdf || !dict)
  {
    if (pdf)
      _pdfioFileError(pdf, "Missing object dictionary.");

    return (NULL);
  }

  if (dict->pdf != pdf)
    dict = pdfioDictCopy(pdf, dict);	// Copy dictionary to new PDF

  // Allocate memory for the object...
  if ((obj = (pdfio_obj_t *)calloc(1, sizeof(pdfio_obj_t))) == NULL)
  {
    _pdfioFileError(pdf, "Unable to allocate memory for object - %s", strerror(errno));
    return (NULL);
  }

  // Expand the objects array as needed
  if (pdf->num_objs >= pdf->alloc_objs)
  {
    pdfio_obj_t **temp = (pdfio_obj_t **)realloc(pdf->objs, (pdf->alloc_objs + 32) * sizeof(pdfio_obj_t *));

    if (!temp)
    {
      _pdfioFileError(pdf, "Unable to allocate memory for object - %s", strerror(errno));
      return (NULL);
    }

    pdf->objs       = temp;
    pdf->alloc_objs += 32;
  }

  pdf->objs[pdf->num_objs ++] = obj;

  // Initialize the object...
  obj->pdf    = pdf;
  obj->number = pdf->num_objs;
  obj->dict   = dict;
  obj->offset = _pdfioFileTell(pdf);

  // Don't write anything just yet...
  return (obj);
}


//
// 'pdfioFileCreatePage()' - Create a page in a PDF file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreatePage(pdfio_file_t *pdf,	// I - PDF file
                    pdfio_dict_t *dict)	// I - Page dictionary
{
  // TODO: Implement me
  (void)pdf;
  (void)dict;
  return (NULL);
}


//
// 'pdfioFileGetName()' - Get a PDF's filename.
//

const char *				// O - Filename
pdfioFileGetName(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf ? pdf->filename : NULL);
}


//
// 'pdfioFileGetNumObjects()' - Get the number of objects in a PDF file.
//

size_t					// O - Number of objects
pdfioFileGetNumObjects(
    pdfio_file_t *pdf)			// I - PDF file
{
  return (pdf ? pdf->num_objs : 0);
}


//
// 'pdfioFileGetNumPages()' - Get the number of pages in a PDF file.
//

size_t					// O - Number of pages
pdfioFileGetNumPages(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf ? pdf->num_pages : 0);
}


//
// 'pdfioFileGetObject()' - Get an object from a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileGetObject(pdfio_file_t *pdf,	// I - PDF file
                   size_t       number)	// I - Object number (starting at 1)
{
  if (!pdf || number < 1 || number > pdf->num_objs)
    return (NULL);
  else
    return (pdf->objs[number - 1]);
}


//
// 'pdfioFileGetPage()' - Get a page object from a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileGetPage(pdfio_file_t *pdf,	// I - PDF file
                 size_t       number)	// I - Page number (starting at 1)
{
  if (!pdf || number < 1 || number > pdf->num_pages)
    return (NULL);
  else
    return (pdf->pages[number - 1]);
}


//
// '()' - Get the PDF version number for a PDF file.
//

const char *				// O - Version number or `NULL`
pdfioFileGetVersion(
    pdfio_file_t *pdf)			// I - PDF file
{
  return (pdf ? pdf->version : NULL);
}


//
// 'pdfioFileOpen()' - Open a PDF file for reading.
//

pdfio_file_t *				// O - PDF file
pdfioFileOpen(
    const char       *filename,		// I - Filename
    pdfio_error_cb_t error_cb,		// I - Error callback or `NULL` for default
    void             *error_data)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file


  // Range check input...
  if (!filename)
    return (NULL);

  if (!error_cb)
  {
    error_cb   = _pdfioFileDefaultError;
    error_data = NULL;
  }

  // Allocate a PDF file structure...
  if ((pdf = (pdfio_file_t *)calloc(1, sizeof(pdfio_file_t))) == NULL)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)filename;
    snprintf(message, sizeof(message), "Unable to allocate memory for PDF file - %s", strerror(errno));
    (error_cb)(&temp, message, error_data);
    return (NULL);
  }

  pdf->filename   = strdup(filename);
  pdf->mode       = _PDFIO_MODE_READ;
  pdf->error_cb   = error_cb;
  pdf->error_data = error_data;

  // Open the file...
  if ((pdf->fd = open(filename, O_RDONLY | O_BINARY)) < 0)
  {
    _pdfioFileError(pdf, "Unable to open file - %s", strerror(errno));
    free(pdf->filename);
    free(pdf);
    return (NULL);
  }

  // TODO: read header, trailer, and xref table...
  return (pdf);
}


//
// 'write_trailer()' - Write the PDF catalog object, xref table, and trailer.
//

static bool				// O - `true` on success, `false` on failure
write_trailer(pdfio_file_t *pdf)	// I - PDF file
{
  // TODO: Write trailer
  (void)pdf;

  return (false);
}
