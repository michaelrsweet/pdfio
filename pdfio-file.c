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

static pdfio_obj_t	*add_object(pdfio_file_t *pdf);
static bool		load_xref(pdfio_file_t *pdf, off_t xref_offset);
static bool		write_trailer(pdfio_file_t *pdf);


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

  if (pdf->mode != _PDFIO_MODE_WRITE)
    return (NULL);

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
      free(obj);
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
// 'pdfioFileGetID()' - Get the PDF file's ID strings.
//

pdfio_array_t *				// O - Array with binary strings
pdfioFileGetID(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf ? pdfioDictGetArray(pdf->trailer, "ID") : NULL);
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
// 'pdfioFileGetVersion()' - Get the PDF version number for a PDF file.
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
  char		line[1024],		// Line from file
		*ptr;			// Pointer into line
  off_t		xref_offset;		// Offset to xref table


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

  // Read the header from the first line...
  if (!_pdfioFileGets(pdf, line, sizeof(line)))
    goto error;

  if ((strncmp(line, "%PDF-1.", 7) && strncmp(line, "%PDF-2.", 7)) || !isdigit(line[7] & 255))
  {
    // Bad header
    _pdfioFileError(pdf, "Bad header '%s'.", line);
    goto error;
  }

  // Copy the version number...
  pdf->version = strdup(line + 5);

  // Grab the last 32 characters of the file to find the start of the xref table...
  _pdfioFileSeek(pdf, -32, SEEK_END);
  if (_pdfioFileRead(pdf, line, 32) < 32)
  {
    _pdfioFileError(pdf, "Unable to read startxref data.");
    goto error;
  }
  line[32] = '\0';

  if ((ptr = strstr(line, "startxref")) == NULL)
  {
    _pdfioFileError(pdf, "Unable to find start of xref table.");
    goto error;
  }

  xref_offset = (off_t)strtol(ptr + 9, NULL, 10);

  if (!load_xref(pdf, xref_offset))
    goto error;

  return (pdf);


  // If we get here we had a fatal read error...
  error:

  pdfioFileClose(pdf);

  return (NULL);
}


//
// 'add_object()' - Add an object to a PDF file.
//

static pdfio_obj_t *			// O - New object
add_object(pdfio_file_t *pdf)		// I - PDF file
{
  pdfio_obj_t	*obj;			// New object


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
      free(obj);
      return (NULL);
    }

    pdf->objs       = temp;
    pdf->alloc_objs += 32;
  }

  pdf->objs[pdf->num_objs ++] = obj;

  return (obj);
}


//
// 'load_xref()' - Load an XREF table...
//

static bool				// O - `true` on success, `false` on failure
load_xref(pdfio_file_t *pdf,		// I - PDF file
          off_t        xref_offset)	// I - Offset to xref
{
  bool		done = false;		// Are we done?
  char		line[1024],		// Line from file
		*ptr;			// Pointer into line
  _pdfio_value_t trailer;		// Trailer dictionary


  while (!done)
  {
    if (_pdfioFileSeek(pdf, xref_offset, SEEK_SET) != xref_offset)
    {
      _pdfioFileError(pdf, "Unable to seek to start of xref table.");
      return (false);
    }

    if (!_pdfioFileGets(pdf, line, sizeof(line)))
    {
      _pdfioFileError(pdf, "Unable to read start of xref table.");
      return (false);
    }

    if (strcmp(line, "xref"))
    {
      _pdfioFileError(pdf, "Bad xref table header '%s'.", line);
      return (false);
    }

    // Read the xref tables
    while (_pdfioFileGets(pdf, line, sizeof(line)))
    {
      intmax_t	number,			// Object number
		num_objects;		// Number of objects

      if (!strcmp(line, "trailer"))
	break;

      if (sscanf(line, "%jd%jd", &number, &num_objects) != 2)
      {
	_pdfioFileError(pdf, "Malformed xref table section '%s'.", line);
	return (false);
      }

      // Read this group of objects...
      for (; num_objects > 0; num_objects --, number ++)
      {
	intmax_t	offset;		// Offset in file
	int		generation;	// Generation number
	pdfio_obj_t	*obj;		// Object

	// Read a line from the file and validate it...
	if (_pdfioFileRead(pdf, line, 20) != 20)
	  return (false);

	line[20] = '\0';

	if (strcmp(line + 18, "\r\n") && strcmp(line + 18, " \n") && strcmp(line + 18, " \r"))
	{
	  _pdfioFileError(pdf, "Malformed xref table entry '%s'.", line);
	  return (false);
	}
	line[18] = '\0';

	// Parse the line
	if ((offset = strtoimax(line, &ptr, 10)) < 0)
	{
	  _pdfioFileError(pdf, "Malformed xref table entry '%s'.", line);
	  return (false);
	}

	if ((generation = (int)strtol(ptr, &ptr, 10)) < 0 || generation > 65535)
	{
	  _pdfioFileError(pdf, "Malformed xref table entry '%s'.", line);
	  return (false);
	}

	if (*ptr != ' ')
	{
	  _pdfioFileError(pdf, "Malformed xref table entry '%s'.", line);
	  return (false);
	}

	ptr ++;
	if (*ptr != 'f' && *ptr != 'n')
	{
	  _pdfioFileError(pdf, "Malformed xref table entry '%s'.", line);
	  return (false);
	}

	if (*ptr == 'f')
	  continue;			// Don't care about free objects...

	// Create a placeholder for the object in memory...
	if ((obj = add_object(pdf)) == NULL)
	  return (false);

	obj->number     = (size_t)number;
	obj->generation = (unsigned short)generation;
	obj->offset     = offset;
      }
    }

    if (strcmp(line, "trailer"))
    {
      _pdfioFileError(pdf, "Missing trailer.");
      return (false);
    }

    if (!_pdfioValueRead(pdf, &trailer))
    {
      _pdfioFileError(pdf, "Unable to read trailer dictionary.");
      return (false);
    }
    else if (trailer.type != PDFIO_VALTYPE_DICT)
    {
      _pdfioFileError(pdf, "Trailer is not a dictionary.");
      return (false);
    }

    PDFIO_DEBUG("load_xref: Contents of trailer dictionary:\n");
    PDFIO_DEBUG_DICT(trailer.value.dict, "load_xref");

    if (!pdf->trailer)
    {
      // Save the trailer dictionary and grab the root (catalog) and info
      // objects...
      pdf->trailer = trailer.value.dict;

      if ((pdf->root = pdfioDictGetObject(pdf->trailer, "Root")) == NULL)
      {
        _pdfioFileError(pdf, "Missing Root object.");
        return (false);
      }

      pdf->info     = pdfioDictGetObject(pdf->trailer, "Info");
      pdf->encrypt  = pdfioDictGetObject(pdf->trailer, "Encrypt");
      pdf->id_array = pdfioDictGetArray(pdf->trailer, "ID");
    }

    if ((xref_offset = (off_t)pdfioDictGetNumber(pdf->trailer, "Prev")) <= 0)
      done = true;
  }

  return (true);
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
