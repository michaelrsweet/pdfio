//
// PDF file functions for PDFio.
//
// Copyright © 2021-2025 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"
#ifndef O_BINARY
#  define O_BINARY 0
#endif // !O_BINARY


//
// Local functions...
//

static pdfio_obj_t	*add_obj(pdfio_file_t *pdf, size_t number, unsigned short generation, off_t offset);
static int		compare_objmaps(_pdfio_objmap_t *a, _pdfio_objmap_t *b);
static pdfio_file_t	*create_common(const char *filename, int fd, pdfio_output_cb_t output_cb, void *output_cbdata, const char *version, pdfio_rect_t *media_box, pdfio_rect_t *crop_box, pdfio_error_cb_t error_cb, void *error_cbdata);
static const char	*get_info_string(pdfio_file_t *pdf, const char *key);
static struct lconv	*get_lconv(void);
static bool		load_obj_stream(pdfio_obj_t *obj);
static bool		load_pages(pdfio_file_t *pdf, pdfio_obj_t *obj, size_t depth);
static bool		load_xref(pdfio_file_t *pdf, off_t xref_offset, pdfio_password_cb_t password_cb, void *password_data);
static bool		repair_xref(pdfio_file_t *pdf, pdfio_password_cb_t password_cb, void *password_data);
static bool		write_pages(pdfio_file_t *pdf);
static bool		write_trailer(pdfio_file_t *pdf);


//
// '_pdfioFileAddMappedObj()' - Add a mapped object.
//

bool					// O - `true` on success, `false` on failure
_pdfioFileAddMappedObj(
    pdfio_file_t *pdf,			// I - Destination PDF file
    pdfio_obj_t  *dst_obj,		// I - Destination object
    pdfio_obj_t  *src_obj)		// I - Source object
{
  _pdfio_objmap_t	*map;		// Object map


  // Allocate memory as needed...
  if (pdf->num_objmaps >= pdf->alloc_objmaps)
  {
    if ((map = realloc(pdf->objmaps, (pdf->alloc_objmaps + 16) * sizeof(_pdfio_objmap_t))) == NULL)
    {
      _pdfioFileError(pdf, "Unable to allocate memory for object map.");
      return (false);
    }

    pdf->alloc_objmaps += 16;
    pdf->objmaps       = map;
  }

  // Add an object to the end...
  map = pdf->objmaps + pdf->num_objmaps;
  pdf->num_objmaps ++;

  map->obj        = dst_obj;
  map->src_pdf    = src_obj->pdf;
  map->src_number = src_obj->number;

  // Sort as needed...
  if (pdf->num_objmaps > 1 && compare_objmaps(map, pdf->objmaps + pdf->num_objmaps - 2) < 0)
    qsort(pdf->objmaps, pdf->num_objmaps, sizeof(_pdfio_objmap_t), (int (*)(const void *, const void *))compare_objmaps);

  return (true);
}


//
// '_pdfioFileAddPage()' - Add a page to a PDF file.
//

bool					// O - `true` on success and `false` on failure
_pdfioFileAddPage(pdfio_file_t *pdf,	// I - PDF file
                  pdfio_obj_t  *obj)	// I - Page object
{
  // Add the page to the array of pages...
  if (pdf->num_pages >= pdf->alloc_pages)
  {
    pdfio_obj_t **temp = (pdfio_obj_t **)realloc(pdf->pages, (pdf->alloc_pages + 16) * sizeof(pdfio_obj_t *));

    if (!temp)
    {
      _pdfioFileError(pdf, "Unable to allocate memory for pages.");
      return (false);
    }

    pdf->alloc_pages += 16;
    pdf->pages       = temp;
  }

  pdf->pages[pdf->num_pages ++] = obj;

  return (true);
}


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
  {
    ret = false;

    if (pdfioObjClose(pdf->info_obj) && write_pages(pdf) && pdfioObjClose(pdf->root_obj) && write_trailer(pdf))
      ret = _pdfioFileFlush(pdf);
  }

  if (pdf->fd >= 0 && close(pdf->fd) < 0)
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

  free(pdf->objmaps);

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
// This function creates a new PDF file.  The "filename" argument specifies the
// name of the PDF file to create.
//
// The "version" argument specifies the PDF version number for the file or
// `NULL` for the default ("2.0").  The value "PCLm-1.0" can be specified to
// produce the PCLm subset of PDF.
//
// The "media_box" and "crop_box" arguments specify the default MediaBox and
// CropBox for pages in the PDF file - if `NULL` then a default "Universal" size
// of 8.27x11in (the intersection of US Letter and ISO A4) is used.
//
// The "error_cb" and "error_cbdata" arguments specify an error handler callback
// and its data pointer - if `NULL` the default error handler is used that
// writes error messages to `stderr`.
//

pdfio_file_t *				// O - PDF file or `NULL` on error
pdfioFileCreate(
    const char       *filename,		// I - Filename
    const char       *version,		// I - PDF version number or `NULL` for default (2.0)
    pdfio_rect_t     *media_box,	// I - Default MediaBox for pages
    pdfio_rect_t     *crop_box,		// I - Default CropBox for pages
    pdfio_error_cb_t error_cb,		// I - Error callback or `NULL` for default
    void             *error_cbdata)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file
  int		fd;			// File descriptor


  PDFIO_DEBUG("pdfioFileCreate(filename=\"%s\", version=\"%s\", media_box=%p, crop_box=%p, error_cb=%p, error_cbdata=%p)\n", filename, version, (void *)media_box, (void *)crop_box, (void *)error_cb, (void *)error_cbdata);

  // Range check input...
  if (!filename)
    return (NULL);

  // Create the file...
  if ((fd = open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0666)) < 0)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)filename;
    snprintf(message, sizeof(message), "Unable to create '%s': %s", filename, strerror(errno));
    (error_cb)(&temp, message, error_cbdata);

    return (NULL);
  }

  if ((pdf = create_common(filename, fd, /*output_cb*/NULL, /*output_cbdata*/NULL, version, media_box, crop_box, error_cb, error_cbdata)) == NULL)
  {
    // Remove the newly created file if we can't create the PDF file object...
    close(fd);
    unlink(filename);
  }

  return (pdf);
}


//
// 'pdfioFileCreateArrayObj()' - Create a new object in a PDF file containing an array.
//
// This function creates a new object with an array value in a PDF file.
// You must call @link pdfioObjClose@ to write the object to the file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateArrayObj(
    pdfio_file_t  *pdf,			// I - PDF file
    pdfio_array_t *array)		// I - Object array
{
  _pdfio_value_t	value;		// Object value


  // Range check input...
  if (!pdf || !array)
    return (NULL);

  value.type        = PDFIO_VALTYPE_ARRAY;
  value.value.array = array;

  return (_pdfioFileCreateObj(pdf, array->pdf, &value));
}


//
// 'pdfioFileCreateNameObj()' - Create a new object in a PDF file containing a name.
//
// This function creates a new object with a name value in a PDF file.
// You must call @link pdfioObjClose@ to write the object to the file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateNameObj(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *name)			// I - Name value
{
  _pdfio_value_t	value;		// Object value


  // Range check input...
  if (!pdf || !name)
    return (NULL);

  value.type         = PDFIO_VALTYPE_NAME;
  value.value.name   = pdfioStringCreate(pdf, name);

  if (!value.value.name)
    return (NULL);

  return (_pdfioFileCreateObj(pdf, NULL, &value));
}


//
// 'pdfioFileCreateNumberObj()' - Create a new object in a PDF file containing a number.
//
// This function creates a new object with a number value in a PDF file.
// You must call @link pdfioObjClose@ to write the object to the file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateNumberObj(
    pdfio_file_t *pdf,			// I - PDF file
    double       number)		// I - Number value
{
  _pdfio_value_t	value;		// Object value


  // Range check input...
  if (!pdf)
    return (NULL);

  value.type         = PDFIO_VALTYPE_NUMBER;
  value.value.number = number;

  return (_pdfioFileCreateObj(pdf, NULL, &value));
}


//
// 'pdfioFileCreateObj()' - Create a new object in a PDF file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateObj(
    pdfio_file_t *pdf,			// I - PDF file
    pdfio_dict_t *dict)			// I - Object dictionary
{
  _pdfio_value_t	value;		// Object value


  // Range check input...
  if (!pdf || !dict)
    return (NULL);

  value.type       = PDFIO_VALTYPE_DICT;
  value.value.dict = dict;

  return (_pdfioFileCreateObj(pdf, dict->pdf, &value));
}


//
// '_pdfioFileCreateObj()' - Create a new object in a PDF file with a value.
//

pdfio_obj_t *				// O - New object
_pdfioFileCreateObj(
    pdfio_file_t   *pdf,		// I - PDF file
    pdfio_file_t   *srcpdf,		// I - Source PDF file, if any
    _pdfio_value_t *value)		// I - Object dictionary
{
  pdfio_obj_t	*obj;			// New object


  // Range check input...
  if (!pdf)
    return (NULL);

  if (pdf->mode != _PDFIO_MODE_WRITE)
    return (NULL);

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

  if (value)
    _pdfioValueCopy(pdf, &obj->value, srcpdf, value);

  // Don't write anything just yet...
  return (obj);
}


//
// 'pdfioFileCreateOutput()' - Create a PDF file through an output callback.
//
// This function creates a new PDF file that is streamed though an output
// callback.  The "output_cb" and "output_cbdata" arguments specify the output
// callback and its data pointer which is called whenever data needs to be
// written:
//
// ```
// ssize_t
// output_cb(void *output_cbdata, const void *buffer, size_t bytes)
// {
//   // Write buffer to output and return the number of bytes written
// }
// ```
//
// The "version" argument specifies the PDF version number for the file or
// `NULL` for the default ("2.0").  Unlike @link pdfioFileCreate@ and
// @link pdfioFileCreateTemporary@, it is generally not safe to pass the
// "PCLm-1.0" version string.
//
// The "media_box" and "crop_box" arguments specify the default MediaBox and
// CropBox for pages in the PDF file - if `NULL` then a default "Universal" size
// of 8.27x11in (the intersection of US Letter and ISO A4) is used.
//
// The "error_cb" and "error_cbdata" arguments specify an error handler callback
// and its data pointer - if `NULL` the default error handler is used that
// writes error messages to `stderr`.
//
// > *Note*: Files created using this API are slightly larger than those
// > created using the @link pdfioFileCreate@ function since stream lengths are
// > stored as indirect object references.
//

pdfio_file_t *				// O - PDF file or `NULL` on error
pdfioFileCreateOutput(
    pdfio_output_cb_t output_cb,	// I - Output callback function
    void              *output_cbdata,	// I - Output callback data
    const char        *version,		// I - PDF version number or `NULL` for default (2.0)
    pdfio_rect_t      *media_box,	// I - Default MediaBox for pages
    pdfio_rect_t      *crop_box,	// I - Default CropBox for pages
    pdfio_error_cb_t  error_cb,		// I - Error callback or `NULL` for default
    void              *error_cbdata)	// I - Error callback data, if any
{
  PDFIO_DEBUG("pdfioFileCreate(output_cb=%p, output_cbdata=%p, version=\"%s\", media_box=%p, crop_box=%p, error_cb=%p, error_cbdata=%p)\n", (void *)output_cb, (void *)output_cbdata, version, (void *)media_box, (void *)crop_box, (void *)error_cb, (void *)error_cbdata);

  return (create_common("output.pdf", /*fd*/-1, output_cb, output_cbdata, version, media_box, crop_box, error_cb, error_cbdata));
}


//
// 'pdfioFileCreatePage()' - Create a page in a PDF file.
//

pdfio_stream_t *			// O - Contents stream
pdfioFileCreatePage(pdfio_file_t *pdf,	// I - PDF file
                    pdfio_dict_t *dict)	// I - Page dictionary
{
  pdfio_obj_t	*page,			// Page object
		*contents;		// Contents object
  pdfio_dict_t	*contents_dict;		// Dictionary for Contents object


  // Range check input...
  if (!pdf)
    return (NULL);

  // Copy the page dictionary...
  if (dict)
    dict = pdfioDictCopy(pdf, dict);
  else
    dict = pdfioDictCreate(pdf);

  if (!dict)
    return (NULL);

  // Make sure the page dictionary has all of the required keys...
  if (!_pdfioDictGetValue(dict, "CropBox"))
    pdfioDictSetRect(dict, "CropBox", &pdf->crop_box);

  if (!_pdfioDictGetValue(dict, "MediaBox"))
    pdfioDictSetRect(dict, "MediaBox", &pdf->media_box);

  pdfioDictSetObj(dict, "Parent", pdf->pages_obj);

  if (!_pdfioDictGetValue(dict, "Resources"))
    pdfioDictSetDict(dict, "Resources", pdfioDictCreate(pdf));

  if (!_pdfioDictGetValue(dict, "Type"))
    pdfioDictSetName(dict, "Type", "Page");

  // Create the page object...
  if ((page = pdfioFileCreateObj(pdf, dict)) == NULL)
    return (NULL);

  // Create a contents object to hold the contents of the page...
  if ((contents_dict = pdfioDictCreate(pdf)) == NULL)
    return (NULL);

#ifndef DEBUG
  pdfioDictSetName(contents_dict, "Filter", "FlateDecode");
#endif // !DEBUG

  if ((contents = pdfioFileCreateObj(pdf, contents_dict)) == NULL)
    return (NULL);

  // Add the contents stream to the pages object and write it...
  pdfioDictSetObj(dict, "Contents", contents);
  if (!pdfioObjClose(page))
    return (NULL);

  if (!_pdfioFileAddPage(pdf, page))
    return (NULL);

  // Create the contents stream...
#ifdef DEBUG
  return (pdfioObjCreateStream(contents, PDFIO_FILTER_NONE));
#else
  return (pdfioObjCreateStream(contents, PDFIO_FILTER_FLATE));
#endif // DEBUG
}


//
// 'pdfioFileCreateStringObj()' - Create a new object in a PDF file containing a string.
//
// This function creates a new object with a string value in a PDF file.
// You must call @link pdfioObjClose@ to write the object to the file.
//

pdfio_obj_t *				// O - New object
pdfioFileCreateStringObj(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *string)		// I - String
{
  _pdfio_value_t	value;		// Object value


  // Range check input...
  if (!pdf)
    return (NULL);

  value.type         = PDFIO_VALTYPE_STRING;
  value.value.string = string;

  return (_pdfioFileCreateObj(pdf, NULL, &value));
}


//
// 'pdfioFileCreateTemporary()' - Create a temporary PDF file.
//
// This function creates a PDF file with a unique filename in the current
// temporary directory.  The temporary file is stored in the string "buffer" an
// will have a ".pdf" extension.
//
// The "version" argument specifies the PDF version number for the file or
// `NULL` for the default ("2.0").  The value "PCLm-1.0" can be specified to
// produce the PCLm subset of PDF.
//
// The "media_box" and "crop_box" arguments specify the default MediaBox and
// CropBox for pages in the PDF file - if `NULL` then a default "Universal" size
// of 8.27x11in (the intersection of US Letter and ISO A4) is used.
//
// The "error_cb" and "error_cbdata" arguments specify an error handler callback
// and its data pointer - if `NULL` the default error handler is used that
// writes error messages to `stderr`.
//
// @since PDFio v1.1@
//

pdfio_file_t *
pdfioFileCreateTemporary(
    char             *buffer,		// I - Filename buffer
    size_t           bufsize,		// I - Size of filename buffer
    const char       *version,		// I - PDF version number or `NULL` for default (2.0)
    pdfio_rect_t     *media_box,	// I - Default MediaBox for pages
    pdfio_rect_t     *crop_box,		// I - Default CropBox for pages
    pdfio_error_cb_t error_cb,		// I - Error callback or `NULL` for default
    void             *error_cbdata)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file
  int		i,			// Looping var
		fd;			// File descriptor
  const char	*tmpdir;		// Temporary directory
#if _WIN32 || defined(__APPLE__)
  char		tmppath[256];		// Temporary directory path
#endif // _WIN32 || __APPLE__
  unsigned	tmpnum;			// Temporary filename number


  PDFIO_DEBUG("pdfioFileCreate(buffer=%p, bufsize=%lu, version=\"%s\", media_box=%p, crop_box=%p, error_cb=%p, error_cbdata=%p)\n", (void *)buffer, (unsigned long)bufsize, version, (void *)media_box, (void *)crop_box, (void *)error_cb, (void *)error_cbdata);

  // Range check input...
  if (!buffer || bufsize < 32)
  {
    if (buffer)
      *buffer = '\0';
    return (NULL);
  }

  // Create the temporary PDF file...
#if _WIN32
  if ((tmpdir = getenv("TEMP")) == NULL)
  {
    GetTempPathA(sizeof(tmppath), tmppath);
    tmpdir = tmppath;
  }

#elif defined(__APPLE__)
  if ((tmpdir = getenv("TMPDIR")) != NULL && access(tmpdir, W_OK))
    tmpdir = NULL;

  if (!tmpdir)
  {
    // Grab the per-process temporary directory for sandboxed apps...
#  ifdef _CS_DARWIN_USER_TEMP_DIR
    if (confstr(_CS_DARWIN_USER_TEMP_DIR, tmppath, sizeof(tmppath)))
      tmpdir = tmppath;
    else
#  endif // _CS_DARWIN_USER_TEMP_DIR
      tmpdir = "/private/tmp";
  }

#else
  if ((tmpdir = getenv("TMPDIR")) == NULL || access(tmpdir, W_OK))
    tmpdir = "/tmp";
#endif // _WIN32

  for (i = 0, fd = -1; i < 1000; i ++)
  {
    _pdfioCryptoMakeRandom((uint8_t *)&tmpnum, sizeof(tmpnum));
    snprintf(buffer, bufsize, "%s/%08x.pdf", tmpdir, tmpnum);
    if ((fd = open(buffer, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC | O_EXCL, 0666)) >= 0)
      break;
  }

  if (fd < 0)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)"<temporary>";
    snprintf(message, sizeof(message), "Unable to create temporary PDF file: %s", strerror(errno));
    (error_cb)(&temp, message, error_cbdata);

    return (NULL);
  }

  if ((pdf = create_common(buffer, fd, /*output_cb*/NULL, /*output_cbdata*/NULL, version, media_box, crop_box, error_cb, error_cbdata)) == NULL)
  {
    // Remove the temporary file if we can't create the PDF file object...
    close(fd);
    unlink(buffer);
    *buffer = '\0';
  }

  return (pdf);
}


//
// '_pdfioFileFindMappedObj()' - Find a mapped object.
//

pdfio_obj_t *				// O - Match object or `NULL` if none
_pdfioFileFindMappedObj(
    pdfio_file_t *pdf,			// I - Destination PDF file
    pdfio_file_t *src_pdf,		// I - Source PDF file
    size_t       src_number)		// I - Source object number
{
  _pdfio_objmap_t	key,		// Search key
			*match;		// Matching object map


  // If we have no mapped objects, return NULL immediately...
  if (pdf->num_objmaps == 0)
    return (NULL);

  // Otherwise search for a match...
  key.src_pdf    = src_pdf;
  key.src_number = src_number;

  if ((match = (_pdfio_objmap_t *)bsearch(&key, pdf->objmaps, pdf->num_objmaps, sizeof(_pdfio_objmap_t), (int (*)(const void *, const void *))compare_objmaps)) != NULL)
    return (match->obj);
  else
    return (NULL);
}


//
// 'pdfioFileFindObj()' - Find an object using its object number.
//
// This differs from @link pdfioFileGetObj@ which takes an index into the
// list of objects while this function takes the object number.
//

pdfio_obj_t *				// O - Object or `NULL` if not found
pdfioFileFindObj(
    pdfio_file_t *pdf,			// I - PDF file
    size_t       number)		// I - Object number (1 to N)
{
  size_t	left,			// Left object
		right,			// Right object
		current;		// Current object


  PDFIO_DEBUG("pdfioFileFindObj(pdf=%p, number=%lu) alloc_objs=%lu, num_objs=%lu, objs=%p\n", (void *)pdf, (unsigned long)number, (unsigned long)(pdf ? pdf->alloc_objs : 0), (unsigned long)(pdf ? pdf->num_objs : 0), (void *)(pdf ? pdf->objs : NULL));

  // Range check input...
  if (!pdf || pdf->num_objs == 0 || number < 1)
    return (NULL);

  // Do a binary search for the object...
  if ((current = number - 1) >= pdf->num_objs)
    current = pdf->num_objs / 2;

  PDFIO_DEBUG("pdfioFileFindObj: objs[current=%lu]=%p(%lu)\n", (unsigned long)current, (void *)pdf->objs[current], (unsigned long)(pdf->objs[current] ? pdf->objs[current]->number : 0));

  if (number == pdf->objs[current]->number)
  {
    // Fast match...
    PDFIO_DEBUG("pdfioFileFindObj: Returning %lu (%p)\n", (unsigned long)current, pdf->objs[current]);
    return (pdf->objs[current]);
  }
  else if (number < pdf->objs[current]->number)
  {
    left  = 0;
    right = current;
  }
  else
  {
    left  = current;
    right = pdf->num_objs - 1;
  }

  while ((right - left) > 1)
  {
    current = (left + right) / 2;

    if (number == pdf->objs[current]->number)
      return (pdf->objs[current]);
    else if (number < pdf->objs[current]->number)
      right = current;
    else
      left = current;
  }

  if (number == pdf->objs[left]->number)
  {
    PDFIO_DEBUG("pdfioFileFindObj: Returning %lu (%p)\n", (unsigned long)left, pdf->objs[left]);
    return (pdf->objs[left]);
  }
  else if (number == pdf->objs[right]->number)
  {
    PDFIO_DEBUG("pdfioFileFindObj: Returning %lu (%p)\n", (unsigned long)right, pdf->objs[right]);
    return (pdf->objs[right]);
  }
  else
  {
    PDFIO_DEBUG("pdfioFileFindObj: Returning NULL\n");
    return (NULL);
  }
}


//
// 'pdfioFileGetAuthor()' - Get the author for a PDF file.
//

const char *				// O - Author or `NULL` for none
pdfioFileGetAuthor(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Author"));
}


//
// 'pdfioFileGetCatalog()' - Get the document catalog dictionary.
//
// @since PDFio 1.3@
//

pdfio_dict_t *				// O - Catalog dictionary
pdfioFileGetCatalog(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf ? pdfioObjGetDict(pdf->root_obj) : NULL);
}


//
// 'pdfioFileGetCreationDate()' - Get the creation date for a PDF file.
//

time_t					// O - Creation date or `0` for none
pdfioFileGetCreationDate(
    pdfio_file_t *pdf)			// I - PDF file
{
  return (pdf && pdf->info_obj ? pdfioDictGetDate(pdfioObjGetDict(pdf->info_obj), "CreationDate") : 0);
}


//
// 'pdfioFileGetCreator()' - Get the creator string for a PDF file.
//

const char *				// O - Creator string or `NULL` for none
pdfioFileGetCreator(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Creator"));
}


//
// 'pdfioFileGetID()' - Get the PDF file's ID strings.
//

pdfio_array_t *				// O - Array with binary strings
pdfioFileGetID(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf ? pdf->id_array : NULL);
}


//
// 'pdfioFileGetKeywords()' - Get the keywords for a PDF file.
//

const char *				// O - Keywords string or `NULL` for none
pdfioFileGetKeywords(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Keywords"));
}


//
// 'pdfioFileGetModificationDate()' - Get the most recent modification date for a PDF file.
//

time_t					// O - Modification date or `0` for none
pdfioFileGetModificationDate(
    pdfio_file_t *pdf)			// I - PDF file
{
  return (pdf && pdf->info_obj ? pdfioDictGetDate(pdfioObjGetDict(pdf->info_obj), "ModDate") : 0);
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
// 'pdfioFileGetNumObjs()' - Get the number of objects in a PDF file.
//

size_t					// O - Number of objects
pdfioFileGetNumObjs(
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
// 'pdfioFileGetObj()' - Get an object from a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileGetObj(pdfio_file_t *pdf,	// I - PDF file
                size_t       n)		// I - Object index (starting at 0)
{
  if (!pdf || n >= pdf->num_objs)
    return (NULL);
  else
    return (pdf->objs[n]);
}


//
// 'pdfioFileGetPage()' - Get a page object from a PDF file.
//

pdfio_obj_t *				// O - Object
pdfioFileGetPage(pdfio_file_t *pdf,	// I - PDF file
                 size_t       n)	// I - Page index (starting at 0)
{
  if (!pdf || n >= pdf->num_pages)
    return (NULL);
  else
    return (pdf->pages[n]);
}


//
// 'pdfioFileGetPermissions()' - Get the access permissions of a PDF file.
//
// This function returns the access permissions of a PDF file and (optionally)
// the type of encryption that has been used.
//

pdfio_permission_t			// O - Permission bits
pdfioFileGetPermissions(
    pdfio_file_t       *pdf,		// I - PDF file
    pdfio_encryption_t *encryption)	// O - Type of encryption used or `NULL` to ignore
{
  // Range check input...
  if (!pdf)
  {
    if (encryption)
      *encryption = PDFIO_ENCRYPTION_NONE;

    return (PDFIO_PERMISSION_ALL);
  }

  // Return values...
  if (encryption)
    *encryption = pdf->encryption;

  return (pdf->permissions);
}


//
// 'pdfioFileGetProducer()' - Get the producer string for a PDF file.
//

const char *				// O - Producer string or `NULL` for none
pdfioFileGetProducer(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Producer"));
}


//
// 'pdfioFileGetSubject()' - Get the subject for a PDF file.
//

const char *				// O - Subject or `NULL` for none
pdfioFileGetSubject(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Subject"));
}


//
// 'pdfioFileGetTitle()' - Get the title for a PDF file.
//

const char *				// O - Title or `NULL` for none
pdfioFileGetTitle(pdfio_file_t *pdf)	// I - PDF file
{
  return (get_info_string(pdf, "Title"));
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
// This function opens an existing PDF file.  The "filename" argument specifies
// the name of the PDF file to create.
//
// The "password_cb" and "password_cbdata" arguments specify a password callback
// and its data pointer for PDF files that use one of the standard Adobe
// "security" handlers.  The callback returns a password string or `NULL` to
// cancel the open.  If `NULL` is specified for the callback function and the
// PDF file requires a password, the open will always fail.
//
// The "error_cb" and "error_cbdata" arguments specify an error handler callback
// and its data pointer - if `NULL` the default error handler is used that
// writes error messages to `stderr`.
//

pdfio_file_t *				// O - PDF file
pdfioFileOpen(
    const char          *filename,	// I - Filename
    pdfio_password_cb_t password_cb,	// I - Password callback or `NULL` for none
    void                *password_cbdata,
					// I - Password callback data, if any
    pdfio_error_cb_t    error_cb,	// I - Error callback or `NULL` for default
    void                *error_cbdata)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file
  char		line[1025],		// Line from file
		*ptr,			// Pointer into line
		*end;			// End of line
  ssize_t	bytes;			// Bytes read
  off_t		xref_offset;		// Offset to xref table


  PDFIO_DEBUG("pdfioFileOpen(filename=\"%s\", password_cb=%p, password_cbdata=%p, error_cb=%p, error_cbdata=%p)\n", filename, (void *)password_cb, (void *)password_cbdata, (void *)error_cb, (void *)error_cbdata);

  // Range check input...
  if (!filename)
    return (NULL);

  if (!error_cb)
  {
    error_cb     = _pdfioFileDefaultError;
    error_cbdata = NULL;
  }

  // Allocate a PDF file structure...
  if ((pdf = (pdfio_file_t *)calloc(1, sizeof(pdfio_file_t))) == NULL)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)filename;
    snprintf(message, sizeof(message), "Unable to allocate memory for PDF file - %s", strerror(errno));
    (error_cb)(&temp, message, error_cbdata);
    return (NULL);
  }

  pdf->loc         = get_lconv();
  pdf->filename    = strdup(filename);
  pdf->mode        = _PDFIO_MODE_READ;
  pdf->error_cb    = error_cb;
  pdf->error_data  = error_cbdata;
  pdf->permissions = PDFIO_PERMISSION_ALL;

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

  // Grab the last 1k of the file to find the start of the xref table...
  if (_pdfioFileSeek(pdf, -1024, SEEK_END) < 0)
  {
    _pdfioFileError(pdf, "Unable to read startxref data.");
    goto error;
  }

  if ((bytes = _pdfioFileRead(pdf, line, sizeof(line) - 1)) < 1)
  {
    _pdfioFileError(pdf, "Unable to read startxref data.");
    goto error;
  }

  line[bytes] = '\0';
  end = line + bytes - 9;

  for (ptr = line; ptr < end; ptr ++)
  {
    if (!memcmp(ptr, "startxref", 9))
      break;
  }

  if (ptr >= end)
  {
    _pdfioFileError(pdf, "Unable to find start of xref table.");
    goto error;
  }

  xref_offset = (off_t)strtol(ptr + 9, NULL, 10);

  if (!load_xref(pdf, xref_offset, password_cb, password_cbdata))
  {
    if (!repair_xref(pdf, password_cb, password_cbdata))
      goto error;
  }

  return (pdf);


  // If we get here we had a fatal read error...
  error:

  pdfioFileClose(pdf);

  return (NULL);
}


//
// 'pdfioFileSetAuthor()' - Set the author for a PDF file.
//

void
pdfioFileSetAuthor(pdfio_file_t *pdf,	// I - PDF file
                   const char   *value)	// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetString(pdf->info_obj->value.value.dict, "Author", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetCreationDate()' - Set the creation date for a PDF file.
//

void
pdfioFileSetCreationDate(
    pdfio_file_t *pdf,			// I - PDF file
    time_t       value)			// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetDate(pdf->info_obj->value.value.dict, "CreationDate", value);
}


//
// 'pdfioFileSetCreator()' - Set the creator string for a PDF file.
//

void
pdfioFileSetCreator(pdfio_file_t *pdf,	// I - PDF file
                    const char   *value)// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetString(pdf->info_obj->value.value.dict, "Creator", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetKeywords()' - Set the keywords string for a PDF file.
//

void
pdfioFileSetKeywords(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *value)		// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetString(pdf->info_obj->value.value.dict, "Keywords", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetModificationDate()' - Set the modification date for a PDF file.
//

void
pdfioFileSetModificationDate(
    pdfio_file_t *pdf,			// I - PDF file
    time_t       value)			// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetDate(pdf->info_obj->value.value.dict, "ModDate", value);
}


//
// 'pdfioFileSetPermissions()' - Set the PDF permissions, encryption mode, and passwords.
//
// This function sets the PDF usage permissions, encryption mode, and
// passwords.
//
// > *Note*: This function must be called before creating or copying any
// > objects.  Due to fundamental limitations in the PDF format, PDF encryption
// > offers little protection from disclosure.  Permissions are not enforced in
// > any meaningful way.
//

bool					// O - `true` on success, `false` otherwise
pdfioFileSetPermissions(
    pdfio_file_t       *pdf,		// I - PDF file
    pdfio_permission_t permissions,	// I - Use permissions
    pdfio_encryption_t encryption,	// I - Type of encryption to use
    const char         *owner_password,	// I - Owner password, if any
    const char         *user_password)	// I - User password, if any
{
  if (!pdf)
    return (false);

  if (pdf->num_objs > 3)		// First three objects are pages, info, and root
  {
    _pdfioFileError(pdf, "You must call pdfioFileSetPermissions before adding any objects.");
    return (false);
  }

  if (encryption == PDFIO_ENCRYPTION_NONE)
    return (true);

  return (_pdfioCryptoLock(pdf, permissions, encryption, owner_password, user_password));
}


//
// 'pdfioFileSetSubject()' - Set the subject for a PDF file.
//

void
pdfioFileSetSubject(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *value)		// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetString(pdf->info_obj->value.value.dict, "Subject", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetTitle()' - Set the title for a PDF file.
//

void
pdfioFileSetTitle(pdfio_file_t *pdf,	// I - PDF file
                  const char   *value)	// I - Value
{
  if (pdf && pdf->info_obj)
    pdfioDictSetString(pdf->info_obj->value.value.dict, "Title", pdfioStringCreate(pdf, value));
}


//
// '_pdfioObjAdd()' - Add an object to a file.
//

static pdfio_obj_t *			// O - Object
add_obj(pdfio_file_t   *pdf,		// I - PDF file
	size_t         number,		// I - Object number
	unsigned short generation,	// I - Object generation
	off_t          offset)		// I - Offset in file
{
  pdfio_obj_t	*obj;			// Object
  size_t	left,			// Left object
		right,			// Right object
		current;		// Current object (center)


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

  obj->pdf        = pdf;
  obj->number     = number;
  obj->generation = generation;
  obj->offset     = offset;

  PDFIO_DEBUG("add_obj: obj=%p, ->pdf=%p, ->number=%lu, ->offset=%lu\n", obj, pdf, (unsigned long)obj->number, (unsigned long)offset);

  // Insert object into array as needed...
  if (pdf->num_objs == 0 || obj->number > pdf->objs[pdf->num_objs - 1]->number)
  {
    // Append object...
    PDFIO_DEBUG("add_obj: Appending at %lu\n", (unsigned long)pdf->num_objs);

    pdf->objs[pdf->num_objs] = obj;
    pdf->last_obj            = pdf->num_objs;
  }
  else
  {
    // Insert object...
    if (obj->number < pdf->objs[pdf->last_obj]->number)
    {
      left  = 0;
      right = pdf->last_obj;
    }
    else
    {
      left  = pdf->last_obj;
      right = pdf->num_objs - 1;
    }

    while ((right - left) > 1)
    {
      current = (left + right) / 2;

      if (obj->number < pdf->objs[current]->number)
        right = current;
      else
        left = current;
    }

    if (obj->number < pdf->objs[left]->number)
      current = left;
    else if (obj->number < pdf->objs[right]->number)
      current = right;
    else
      current = right;

    PDFIO_DEBUG("add_obj: Inserting at %lu\n", (unsigned long)current);

    if (current < pdf->num_objs)
      memmove(pdf->objs + current + 1, pdf->objs + current, (pdf->num_objs - current) * sizeof(pdfio_obj_t *));

    pdf->objs[current] = obj;
    pdf->last_obj      = current;
  }

  pdf->num_objs ++;

  return (obj);
}


//
// 'compare_objmaps()' - Compare two object maps...
//

static int				// O - Result of comparison
compare_objmaps(_pdfio_objmap_t *a,	// I - First object map
                _pdfio_objmap_t *b)	// I - Second object map
{
  if (a->src_pdf < b->src_pdf)
    return (-1);
  else if (a->src_pdf > b->src_pdf)
    return (1);
  else if (a->src_number < b->src_number)
    return (-1);
  else if (a->src_number > b->src_number)
    return (1);
  else
    return (0);
}


//
// 'create_common()' - Allocate and initialize a pdfio_file_t object for writing.
//

static pdfio_file_t *			// O - New PDF file
create_common(
    const char        *filename,	// I - Filename
    int               fd,		// I - File descriptor, if any
    pdfio_output_cb_t output_cb,	// I - Output callback function, if any
    void              *output_cbdata,	// I - Output callback data, if any
    const char        *version,		// I - PDF version
    pdfio_rect_t      *media_box,	// I - Media box or `NULL` for default
    pdfio_rect_t      *crop_box,	// I - Crop box of `NULL` for default
    pdfio_error_cb_t  error_cb,		// I - Error callback function
    void              *error_cbdata)	// I - Error callback data
{
  pdfio_file_t	*pdf;			// New PDF file
  pdfio_dict_t	*dict;			// Dictionary
  unsigned char	id_value[16];		// File ID value


  PDFIO_DEBUG("create_common(filename=\"%s\", fd=%d, output_cb=%p, output_cbdata=%p, version=\"%s\", media_box=%p, crop_box=%p, error_cb=%p, error_cbdata=%p)\n", filename, fd, (void *)output_cb, (void *)output_cbdata, version, (void *)media_box, (void *)crop_box, (void *)error_cb, (void *)error_cbdata);

  // Range check input...
  if (!filename || (fd < 0 && !output_cb))
    return (NULL);

  if (!version)
    version = "2.0";

  if (!error_cb)
  {
    error_cb     = _pdfioFileDefaultError;
    error_cbdata = NULL;
  }

  // Allocate a PDF file structure...
  if ((pdf = (pdfio_file_t *)calloc(1, sizeof(pdfio_file_t))) == NULL)
  {
    pdfio_file_t temp;			// Dummy file
    char	message[8192];		// Message string

    temp.filename = (char *)filename;
    snprintf(message, sizeof(message), "Unable to allocate memory for PDF file: %s", strerror(errno));
    (error_cb)(&temp, message, error_cbdata);

    return (NULL);
  }

  // Initialize PDF object...
  pdf->loc         = get_lconv();
  pdf->fd          = fd;
  pdf->output_cb   = output_cb;
  pdf->output_ctx  = output_cbdata;
  pdf->filename    = strdup(filename);
  pdf->version     = strdup(!strncmp(version, "PCLm-", 5) ? "1.4" : version);
  pdf->mode        = _PDFIO_MODE_WRITE;
  pdf->error_cb    = error_cb;
  pdf->error_data  = error_cbdata;
  pdf->permissions = PDFIO_PERMISSION_ALL;
  pdf->bufptr      = pdf->buffer;
  pdf->bufend      = pdf->buffer + sizeof(pdf->buffer);

  if (media_box)
  {
    pdf->media_box = *media_box;
  }
  else
  {
    // Default to "universal" size (intersection of A4 and US Letter)
    pdf->media_box.x2 = 210.0 * 72.0f / 25.4f;
    pdf->media_box.y2 = 11.0f * 72.0f;
  }

  if (crop_box)
  {
    pdf->crop_box = *crop_box;
  }
  else
  {
    // Default to "universal" size (intersection of A4 and US Letter)
    pdf->crop_box.x2 = 210.0 * 72.0f / 25.4f;
    pdf->crop_box.y2 = 11.0f * 72.0f;
  }

  // Write a standard PDF header...
  if (!strncmp(version, "PCLm-", 5))
  {
    if (!_pdfioFilePrintf(pdf, "%%PDF-1.4\n%%%s\n", version))
      goto error;
  }
  else if (!_pdfioFilePrintf(pdf, "%%PDF-%s\n%%\342\343\317\323\n", version))
  {
    goto error;
  }

  // Create the pages object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    goto error;

  pdfioDictSetName(dict, "Type", "Pages");

  if ((pdf->pages_obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    goto error;

  // Create the info object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    goto error;

  pdfioDictSetDate(dict, "CreationDate", time(NULL));
  pdfioDictSetString(dict, "Producer", "pdfio/" PDFIO_VERSION);

  if ((pdf->info_obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    goto error;

  // Create the root object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
    goto error;

  pdfioDictSetName(dict, "Type", "Catalog");
  pdfioDictSetObj(dict, "Pages", pdf->pages_obj);

  if ((pdf->root_obj = pdfioFileCreateObj(pdf, dict)) == NULL)
    goto error;

  // Create random file ID values...
  _pdfioCryptoMakeRandom(id_value, sizeof(id_value));

  if ((pdf->id_array = pdfioArrayCreate(pdf)) != NULL)
  {
    pdfioArrayAppendBinary(pdf->id_array, id_value, sizeof(id_value));
    pdfioArrayAppendBinary(pdf->id_array, id_value, sizeof(id_value));
  }

  return (pdf);

  // Common error handling code...
  error:

  pdfioFileClose(pdf);

  return (NULL);
}


//
// 'get_info_string()' - Get a string value from the Info dictionary.
//
// This function also handles converting binary strings to C strings, which
// occur in encrypted PDF files.
//

static const char *			// O - String or `NULL` if not found
get_info_string(pdfio_file_t *pdf,	// I - PDF file
                const char   *key)	// I - Dictionary key
{
  pdfio_dict_t	*dict;			// Info dictionary


  // Range check input...
  if (!pdf || !pdf->info_obj || (dict = pdfioObjGetDict(pdf->info_obj)) == NULL)
    return (NULL);
  else
    return (pdfioDictGetString(dict, key));
}


//
// 'get_lconv()' - Get any locale-specific numeric information.
//

static struct lconv *			// O - Locale information or `NULL`
get_lconv(void)
{
  struct lconv	*loc;			// Locale information


  if ((loc = localeconv()) != NULL)
  {
    PDFIO_DEBUG("get_lconv: loc=%p, loc->decimal_point=\"%s\"\n", loc, loc->decimal_point);

    if (!loc->decimal_point || !strcmp(loc->decimal_point, "."))
      loc = NULL;
  }

  return (loc);
}


//
// 'load_obj_stream()' - Load an object stream.
//
// Object streams are Adobe's complicated solution for saving a few
// kilobytes in an average PDF file at the expense of massively more
// complicated reader applications.
//
// Each object stream starts with pairs of object numbers and offsets,
// followed by the object values (typically dictionaries).  For
// simplicity pdfio loads all of these values into memory so that we
// don't later have to randomly access compressed stream data to get
// a dictionary.
//

static bool				// O - `true` on success, `false` on error
load_obj_stream(pdfio_obj_t *obj)	// I - Object to load
{
  pdfio_stream_t	*st;		// Stream
  _pdfio_token_t	tb;		// Token buffer/stack
  char			buffer[32];	// Token
  size_t		number,		// Object number
			cur_obj,	// Current object
			num_objs = 0;	// Number of objects
  pdfio_obj_t		*objs[16384];	// Objects
  int			count;		// Count of objects


  PDFIO_DEBUG("load_obj_stream(obj=%p(%d))\n", obj, (int)obj->number);

  // Open the object stream...
  if ((st = pdfioObjOpenStream(obj, true)) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to open compressed object stream %lu.", (unsigned long)obj->number);
    return (false);
  }

  count = (int)pdfioDictGetNumber(pdfioObjGetDict(obj), "N");

  PDFIO_DEBUG("load_obj_stream: N=%d\n", count);

  _pdfioTokenInit(&tb, obj->pdf, (_pdfio_tconsume_cb_t)pdfioStreamConsume, (_pdfio_tpeek_cb_t)pdfioStreamPeek, st);

  // Read the object numbers from the beginning of the stream...
  while (count > 0 && _pdfioTokenGet(&tb, buffer, sizeof(buffer)))
  {
    // Stop if this isn't an object number...
    PDFIO_DEBUG("load_obj_stream: %s\n", buffer);
    if (!isdigit(buffer[0] & 255))
      break;

    // Stop if we have too many objects...
    if (num_objs >= (sizeof(objs) / sizeof(objs[0])))
    {
      _pdfioFileError(obj->pdf, "Too many compressed objects in one stream.");
      pdfioStreamClose(st);
      return (false);
    }

    // Add the object in memory...
    number = (size_t)strtoimax(buffer, NULL, 10);

    if ((objs[num_objs] = pdfioFileFindObj(obj->pdf, number)) == NULL)
      objs[num_objs] = add_obj(obj->pdf, number, 0, 0);

    num_objs ++;

    // Skip offset
    _pdfioTokenGet(&tb, buffer, sizeof(buffer));
    PDFIO_DEBUG("load_obj_stream: %ld at offset %s\n", (long)number, buffer);

    // One less compressed object...
    count --;
  }

  PDFIO_DEBUG("load_obj_stream: num_objs=%lu\n", (unsigned long)num_objs);

  // Read the objects themselves...
  for (cur_obj = 0; cur_obj < num_objs; cur_obj ++)
  {
    if (!_pdfioValueRead(obj->pdf, obj, &tb, &(objs[cur_obj]->value), 0))
    {
      _pdfioFileError(obj->pdf, "Unable to read compressed object.");
      pdfioStreamClose(st);
      return (false);
    }
  }

  // Close the stream and return
  pdfioStreamClose(st);

  return (true);
}


//
// 'load_pages()' - Load pages in the document.
//

static bool				// O - `true` on success, `false` on error
load_pages(pdfio_file_t *pdf,		// I - PDF file
           pdfio_obj_t  *obj,		// I - Page object
           size_t       depth)		// I - Depth of page tree
{
  pdfio_dict_t	*dict;			// Page object dictionary
  const char	*type;			// Node type
  pdfio_array_t	*kids;			// Kids array


  // Range check input...
  if (!obj)
  {
    _pdfioFileError(pdf, "Unable to find pages object.");
    return (false);
  }

  // Get the object dictionary and make sure this is a Pages or Page object...
  if ((dict = pdfioObjGetDict(obj)) == NULL)
  {
    _pdfioFileError(pdf, "No dictionary for pages object.");
    return (false);
  }

  if ((type = pdfioDictGetName(dict, "Type")) == NULL || (strcmp(type, "Pages") && strcmp(type, "Page")))
    return (false);

  // If there is a Kids array, then this is a parent node and we have to look
  // at the child objects...
  if ((kids = pdfioDictGetArray(dict, "Kids")) != NULL)
  {
    // Load the child objects...
    size_t	i,			// Looping var
		num_kids;		// Number of elements in array

    if (depth >= PDFIO_MAX_DEPTH)
    {
      _pdfioFileError(pdf, "Depth of pages objects too great to load.");
      return (false);
    }

    for (i = 0, num_kids = pdfioArrayGetSize(kids); i < num_kids; i ++)
    {
      if (!load_pages(pdf, pdfioArrayGetObj(kids, i), depth + 1))
        return (false);
    }
  }
  else
  {
    // Add this page...
    if (pdf->num_pages >= pdf->alloc_pages)
    {
      pdfio_obj_t **temp = (pdfio_obj_t **)realloc(pdf->pages, (pdf->alloc_pages + 32) * sizeof(pdfio_obj_t *));

      if (!temp)
      {
        _pdfioFileError(pdf, "Unable to allocate memory for pages.");
        return (false);
      }

      pdf->alloc_pages += 32;
      pdf->pages       = temp;
    }

    pdf->pages[pdf->num_pages ++] = obj;
  }

  return (true);
}


//
// 'load_xref()' - Load an XREF table...
//

static bool				// O - `true` on success, `false` on failure
load_xref(
    pdfio_file_t        *pdf,		// I - PDF file
    off_t               xref_offset,	// I - Offset to xref
    pdfio_password_cb_t password_cb,	// I - Password callback or `NULL` for none
    void                *password_data)	// I - Password callback data, if any
{
  bool		done = false;		// Are we done?
  char		line[1024],		// Line from file
		*ptr;			// Pointer into line
  _pdfio_value_t trailer;		// Trailer dictionary
  intmax_t	number,			// Object number
		num_objects,		// Number of objects
		offset;			// Offset in file
  int		generation;		// Generation number
  _pdfio_token_t tb;			// Token buffer/stack
  off_t		line_offset;		// Offset to start of line


  while (!done)
  {
    if (_pdfioFileSeek(pdf, xref_offset, SEEK_SET) != xref_offset)
    {
      _pdfioFileError(pdf, "Unable to seek to start of xref table.");
      return (false);
    }

    do
    {
      line_offset = _pdfioFileTell(pdf);

      if (!_pdfioFileGets(pdf, line, sizeof(line)))
      {
	_pdfioFileError(pdf, "Unable to read start of xref table.");
	return (false);
      }
    }
    while (!line[0]);

    PDFIO_DEBUG("load_xref: line_offset=%lu, line='%s'\n", (unsigned long)line_offset, line);

    if (isdigit(line[0] & 255) && strlen(line) > 4 && (!strcmp(line + strlen(line) - 4, " obj") || ((ptr = strstr(line, " obj")) != NULL && ptr[4] == '<')))
    {
      // Cross-reference stream
      pdfio_obj_t	*obj;		// Object
      size_t		i;		// Looping var
      pdfio_array_t	*index_array;	// Index array
      size_t		index_n,	// Current element in array
			index_count,	// Number of values in index array
			count;		// Number of objects in current pairing
      pdfio_array_t	*w_array;	// W array
      size_t		w[3];		// Size of each cross-reference field
      size_t		w_2,		// Offset to second field
			w_3;		// Offset to third field
      size_t		w_total;	// Total length
      pdfio_stream_t	*st;		// Stream
      unsigned char	buffer[32];	// Read buffer
      size_t		num_sobjs = 0,	// Number of object streams
			sobjs[16384];	// Object streams to load
      pdfio_obj_t	*current;	// Current object

      if ((number = strtoimax(line, &ptr, 10)) < 1)
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      if ((generation = (int)strtol(ptr, &ptr, 10)) < 0 || (generation > 65535 && number != 0))
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      while (isspace(*ptr & 255))
	ptr ++;

      if (strncmp(ptr, "obj", 3))
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      if (_pdfioFileSeek(pdf, line_offset + ptr + 3 - line, SEEK_SET) < 0)
      {
        _pdfioFileError(pdf, "Unable to seek to xref object %lu %u.", (unsigned long)number, (unsigned)generation);
        return (false);
      }

      PDFIO_DEBUG("load_xref: Loading object %lu %u.\n", (unsigned long)number, (unsigned)generation);

      if ((obj = add_obj(pdf, (size_t)number, (unsigned short)generation, xref_offset)) == NULL)
      {
        _pdfioFileError(pdf, "Unable to allocate memory for object.");
        return (false);
      }

      _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);

      if (!_pdfioValueRead(pdf, obj, &tb, &trailer, 0))
      {
        _pdfioFileError(pdf, "Unable to read cross-reference stream dictionary.");
        return (false);
      }
      else if (trailer.type != PDFIO_VALTYPE_DICT)
      {
	_pdfioFileError(pdf, "Cross-reference stream does not have a dictionary.");
	return (false);
      }

      obj->value = trailer;

      if (!_pdfioTokenGet(&tb, line, sizeof(line)) || strcmp(line, "stream"))
      {
        _pdfioFileError(pdf, "Unable to get stream after xref dictionary.");
        return (false);
      }

      PDFIO_DEBUG("load_xref: tb.bufptr=%p, tb.bufend=%p, tb.bufptr[0]=0x%02x, tb.bufptr[0]=0x%02x\n", tb.bufptr, tb.bufend, tb.bufptr[0], tb.bufptr[1]);
      if (tb.bufptr && tb.bufptr < tb.bufend && (tb.bufptr[0] == 0x0d || tb.bufptr[0] == 0x0a))
	tb.bufptr ++;			// Skip trailing CR or LF after token

      _pdfioTokenFlush(&tb);

      obj->stream_offset = _pdfioFileTell(pdf);

      if ((index_array = pdfioDictGetArray(trailer.value.dict, "Index")) != NULL)
        index_count = index_array->num_values;
      else
        index_count = 1;

      if ((w_array = pdfioDictGetArray(trailer.value.dict, "W")) == NULL)
      {
	_pdfioFileError(pdf, "Cross-reference stream does not have required W key.");
	return (false);
      }

      w[0]    = (size_t)pdfioArrayGetNumber(w_array, 0);
      w[1]    = (size_t)pdfioArrayGetNumber(w_array, 1);
      w[2]    = (size_t)pdfioArrayGetNumber(w_array, 2);
      w_total = w[0] + w[1] + w[2];
      w_2     = w[0];
      w_3     = w[0] + w[1];

      if (w[1] == 0 || w[2] > 4 || w[0] > sizeof(buffer) || w[1] > sizeof(buffer) || w[2] > sizeof(buffer) || w_total > sizeof(buffer))
      {
	_pdfioFileError(pdf, "Cross-reference stream has invalid W key [%u %u %u].", (unsigned)w[0], (unsigned)w[1], (unsigned)w[2]);
	return (false);
      }

      if ((st = pdfioObjOpenStream(obj, true)) == NULL)
      {
	_pdfioFileError(pdf, "Unable to open cross-reference stream.");
	return (false);
      }

      for (index_n = 0; index_n < index_count; index_n += 2)
      {
        if (index_count == 1)
        {
          number = 0;
          count  = 999999999;
	}
	else
	{
          number = (intmax_t)pdfioArrayGetNumber(index_array, index_n);
          count  = (size_t)pdfioArrayGetNumber(index_array, index_n + 1);
	}

	while (count > 0 && pdfioStreamRead(st, buffer, w_total) > 0)
	{
	  count --;

	  PDFIO_DEBUG("load_xref: number=%u %02X%02X%02X%02X%02X\n", (unsigned)number, buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

	  // Check whether this is an object definition...
	  if (w[0] > 0)
	  {
	    if (buffer[0] == 0)
	    {
	      // Ignore free objects...
	      number ++;
	      continue;
	    }
	  }

          // Offset
	  for (i = 1, offset = buffer[w_2]; i < w[1]; i ++)
	    offset = (offset << 8) | buffer[w_2 + i];

          // Generation number
	  switch (w[2])
	  {
	    default :
		generation = 0;
		break;
	    case 1 :
		generation = buffer[w_3];
		break;
	    case 2 :
		generation = (buffer[w_3] << 8) | buffer[w_3 + 1];
		break;
	    case 3 :
	        // Issue #46: Stupid Microsoft PDF generator using 3 bytes to
	        // encode 16-bit generation numbers == 0 (probably a lazy coder
	        // stuffing things into an array of 64-bit unsigned integers)
		generation = (buffer[w_3] << 16) | (buffer[w_3 + 1] << 8) | buffer[w_3 + 2];
		if (generation > 65535)
		  generation = 65535;
		break;
	    case 4 : // Even stupider :)
		generation = (buffer[w_3] << 24) | (buffer[w_3 + 1] << 16) | (buffer[w_3 + 2] << 8) | buffer[w_3 + 3];
		if (generation > 65535)
		  generation = 65535;
		break;
	  }

	  // Create a placeholder for the object in memory...
	  if ((current = pdfioFileFindObj(pdf, (size_t)number)) != NULL)
	  {
	    PDFIO_DEBUG("load_xref: existing object, prev offset=%u\n", (unsigned)current->offset);

            if (w[0] == 0 || buffer[0] == 1)
            {
              // Location of object...
	      current->offset = offset;
	    }
	    else if (number != offset)
	    {
	      // Object is part of a stream, offset is the object number...
	      current->offset = 0;
	    }

	    PDFIO_DEBUG("load_xref: new offset=%u\n", (unsigned)current->offset);
	  }

	  if (w[0] > 0 && buffer[0] == 2)
	  {
	    // Object streams need to be loaded into memory, so add them
	    // to the list of objects to load later as needed...
	    for (i = 0; i < num_sobjs; i ++)
	    {
	      if (sobjs[i] == (size_t)offset)
		break;
	    }

	    if (i >= num_sobjs)
	    {
	      if (num_sobjs < (sizeof(sobjs) / sizeof(sobjs[0])))
	      {
		sobjs[num_sobjs ++] = (size_t)offset;
	      }
	      else
	      {
		_pdfioFileError(pdf, "Too many object streams.");
	        return (false);
	      }
	    }
	  }
	  else if (!current)
	  {
	    // Add this object...
	    if (!add_obj(pdf, (size_t)number, (unsigned short)generation, offset))
	      return (false);
	  }

	  number ++;
	}
      }

      pdfioStreamClose(st);

      if (!pdf->trailer_dict)
      {
	// Save the trailer dictionary and grab the root (catalog) and info
	// objects...
	pdf->trailer_dict = trailer.value.dict;
	pdf->encrypt_obj  = pdfioDictGetObj(pdf->trailer_dict, "Encrypt");
	pdf->id_array     = pdfioDictGetArray(pdf->trailer_dict, "ID");

	// If the trailer contains an Encrypt key, try unlocking the file...
	if (pdf->encrypt_obj && !_pdfioCryptoUnlock(pdf, password_cb, password_data))
	  return (false);
      }

      // Load any object streams that are left...
      PDFIO_DEBUG("load_xref: %lu compressed object streams to load.\n", (unsigned long)num_sobjs);

      for (i = 0; i < num_sobjs; i ++)
      {
        if ((obj = pdfioFileFindObj(pdf, sobjs[i])) != NULL)
        {
	  PDFIO_DEBUG("load_xref: Loading compressed object stream %lu (pdf=%p, obj->pdf=%p).\n", (unsigned long)sobjs[i], pdf, obj->pdf);

          if (!load_obj_stream(obj))
            return (false);
	}
	else
	{
	  _pdfioFileError(pdf, "Unable to find compressed object stream %lu.", (unsigned long)sobjs[i]);
	  return (false);
	}
      }
    }
    else if (!strncmp(line, "xref", 4) && (!line[4] || isspace(line[4] & 255)))
    {
      // Read the xref tables
      off_t trailer_offset = _pdfioFileTell(pdf);
					// Offset of current line

      PDFIO_DEBUG("load_xref: Reading xref table starting at offset %lu\n", (unsigned long)trailer_offset);
      while (_pdfioFileGets(pdf, line, sizeof(line)))
      {
        PDFIO_DEBUG("load_xref: '%s' at offset %lu\n", line, (unsigned long)trailer_offset);

	if (!strncmp(line, "trailer", 7) && (!line[7] || isspace(line[7] & 255) || line[7] == '<'))
	{
	  if (line[7])
	  {
	    // Probably the start of the trailer dictionary, rewind the file so
	    // we can read it...
	    _pdfioFileSeek(pdf, trailer_offset + 7, SEEK_SET);
	  }

	  break;
	}
	else
	{
	  trailer_offset = _pdfioFileTell(pdf);

          if (!line[0])
	    continue;
	}

	if (sscanf(line, "%jd%jd", &number, &num_objects) != 2)
	{
	  _pdfioFileError(pdf, "Malformed xref table section '%s'.", line);
	  return (false);
	}

	// Read this group of objects...
	for (; num_objects > 0; num_objects --, number ++)
	{
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

	  if ((generation = (int)strtol(ptr, &ptr, 10)) < 0 || (generation > 65535 && offset != 0))
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
	  if (pdfioFileFindObj(pdf, (size_t)number))
	    continue;			// Don't replace newer object...

	  if (!add_obj(pdf, (size_t)number, (unsigned short)generation, offset))
	    return (false);
	}

	trailer_offset = _pdfioFileTell(pdf);
      }

      if (strncmp(line, "trailer", 7))
      {
	_pdfioFileError(pdf, "Missing trailer.");
	return (false);
      }

      _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);

      if (!_pdfioValueRead(pdf, NULL, &tb, &trailer, 0))
      {
	_pdfioFileError(pdf, "Unable to read trailer dictionary.");
	return (false);
      }
      else if (trailer.type != PDFIO_VALTYPE_DICT)
      {
	_pdfioFileError(pdf, "Trailer is not a dictionary.");
	return (false);
      }

      PDFIO_DEBUG("load_xref: Got trailer dict.\n");

      _pdfioTokenFlush(&tb);

      if (!pdf->trailer_dict)
      {
	// Save the trailer dictionary and grab the root (catalog) and info
	// objects...
	pdf->trailer_dict = trailer.value.dict;
	pdf->encrypt_obj  = pdfioDictGetObj(pdf->trailer_dict, "Encrypt");
	pdf->id_array     = pdfioDictGetArray(pdf->trailer_dict, "ID");

	// If the trailer contains an Encrypt key, try unlocking the file...
	if (pdf->encrypt_obj && !_pdfioCryptoUnlock(pdf, password_cb, password_data))
	  return (false);
      }
    }
    else
    {
      _pdfioFileError(pdf, "Bad xref table header '%s'.", line);
      return (false);
    }

    PDFIO_DEBUG("load_xref: Contents of trailer dictionary:\n");
    PDFIO_DEBUG("load_xref: ");
    PDFIO_DEBUG_VALUE(&trailer);
    PDFIO_DEBUG("\n");

    off_t new_offset = (off_t)pdfioDictGetNumber(trailer.value.dict, "Prev");

    if (new_offset <= 0)
    {
      done = true;
    }
    else if (new_offset == xref_offset)
    {
      _pdfioFileError(pdf, "Recursive xref table.");
      return (false);
    }

    xref_offset = new_offset;
  }

  // Once we have all of the xref tables loaded, get the important objects and
  // build the pages array...
  pdf->info_obj = pdfioDictGetObj(pdf->trailer_dict, "Info");

  if ((pdf->root_obj = pdfioDictGetObj(pdf->trailer_dict, "Root")) == NULL)
  {
    _pdfioFileError(pdf, "Missing Root object.");
    return (false);
  }

  PDFIO_DEBUG("load_xref: Root=%p(%lu)\n", pdf->root_obj, (unsigned long)pdf->root_obj->number);

  return (load_pages(pdf, pdfioDictGetObj(pdfioObjGetDict(pdf->root_obj), "Pages"), 0));
}


//
// 'repair_xref()' - Try to "repair" a PDF file and its cross-references...
//

static bool				// O - `true` on success, `false` on failure
repair_xref(
    pdfio_file_t        *pdf,		// I - PDF file
    pdfio_password_cb_t password_cb,	// I - Password callback or `NULL` for none
    void                *password_data)	// I - Password callback data, if any
{
  char		line[16384],		// Line from file
		*ptr;			// Pointer into line
  off_t		line_offset;		// Offset in file
  intmax_t	number;			// Object number
  int		generation;		// Generation number
  size_t	i;			// Looping var
  size_t	num_sobjs = 0;		// Number of object streams
  pdfio_obj_t	*sobjs[16384];		// Object streams to load


  // Read from the beginning of the file, looking for
  if ((line_offset = _pdfioFileSeek(pdf, 0, SEEK_SET)) < 0)
    return (false);

  while (_pdfioFileGets(pdf, line, sizeof(line)))
  {
    // See if this is the start of an object...
    if (line[0] >= '1' && line[0] <= '9')
    {
      // Maybe, look some more...
      if ((number = strtoimax(line, &ptr, 10)) >= 1 && (generation = (int)strtol(ptr, &ptr, 10)) >= 0 && generation < 65536)
      {
        while (isspace(*ptr & 255))
	  ptr ++;

	if (!strncmp(ptr, "obj", 3))
	{
	  // Yes, start of an object...
	  pdfio_obj_t	*obj;		// Object
	  _pdfio_token_t tb;		// Token buffer/stack

          PDFIO_DEBUG("OBJECT %ld %d at offset %ld\n", (long)number, generation, (long)line_offset);

	  if ((obj = add_obj(pdf, (size_t)number, (unsigned short)generation, line_offset)) == NULL)
	  {
	    _pdfioFileError(pdf, "Unable to allocate memory for object.");
	    return (false);
	  }

	  _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);

	  if (!_pdfioValueRead(pdf, obj, &tb, &obj->value, 0))
	  {
	    _pdfioFileError(pdf, "Unable to read cross-reference stream dictionary.");
	    return (false);
	  }

	  if (_pdfioTokenGet(&tb, line, sizeof(line)) && strcmp(line, "stream"))
	  {
	    const char *type = pdfioObjGetType(obj);
					// Object type

	    _pdfioTokenFlush(&tb);
	    obj->stream_offset = _pdfioFileTell(pdf);

	    if (type && !strcmp(type, "ObjStm") && num_sobjs < (sizeof(sobjs) / sizeof(sobjs[0])))
	    {
	      sobjs[num_sobjs] = obj;
	      num_sobjs ++;
	    }

            if (type && !strcmp(type, "XRef") && !pdf->trailer_dict)
            {
              // Save the trailer dictionary...
	      pdf->trailer_dict = pdfioObjGetDict(obj);
	      pdf->encrypt_obj  = pdfioDictGetObj(pdf->trailer_dict, "Encrypt");
	      pdf->id_array     = pdfioDictGetArray(pdf->trailer_dict, "ID");
            }
	  }
	}
      }
    }
    else if (!strncmp(line, "trailer", 7) && (!line[7] || isspace(line[7] & 255) || line[7] == '<'))
    {
      // Trailer dictionary
      _pdfio_token_t tb;		// Token buffer/stack
      _pdfio_value_t trailer;		// Trailer

      if (line[7])
      {
	// Probably the start of the trailer dictionary, rewind the file so
	// we can read it...
	_pdfioFileSeek(pdf, line_offset + 7, SEEK_SET);
      }

      PDFIO_DEBUG("TRAILER at offset %ld\n", (long)line_offset);

      _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);
      if (!_pdfioValueRead(pdf, NULL, &tb, &trailer, 0))
      {
	_pdfioFileError(pdf, "Unable to read cross-reference stream dictionary.");
	return (false);
      }
      else if (trailer.type != PDFIO_VALTYPE_DICT)
      {
	_pdfioFileError(pdf, "Trailer is not a dictionary.");
	return (false);
      }

      _pdfioTokenFlush(&tb);

      if (!pdf->trailer_dict)
      {
	// Save the trailer dictionary and grab the root (catalog) and info
	// objects...
	pdf->trailer_dict = trailer.value.dict;
	pdf->encrypt_obj  = pdfioDictGetObj(pdf->trailer_dict, "Encrypt");
	pdf->id_array     = pdfioDictGetArray(pdf->trailer_dict, "ID");
      }
    }

    // Get the offset for the next line...
    line_offset = _pdfioFileTell(pdf);
  }

  // If the trailer contains an Encrypt key, try unlocking the file...
  if (pdf->encrypt_obj && !_pdfioCryptoUnlock(pdf, password_cb, password_data))
    return (false);

  // Load any stream objects...
  for (i = 0; i < num_sobjs; i ++)
  {
    if (!load_obj_stream(sobjs[i]))
      return (false);
  }

  // Once we have all of the xref tables loaded, get the important objects and
  // build the pages array...
  pdf->info_obj = pdfioDictGetObj(pdf->trailer_dict, "Info");

  if ((pdf->root_obj = pdfioDictGetObj(pdf->trailer_dict, "Root")) == NULL)
  {
    _pdfioFileError(pdf, "Missing Root object.");
    return (false);
  }

  PDFIO_DEBUG("repair_xref: Root=%p(%lu)\n", pdf->root_obj, (unsigned long)pdf->root_obj->number);

  // Load pages...
  return (load_pages(pdf, pdfioDictGetObj(pdfioObjGetDict(pdf->root_obj), "Pages"), 0));
}


//
// 'write_pages()' - Write the PDF pages objects.
//

static bool				// O - `true` on success, `false` on failure
write_pages(pdfio_file_t *pdf)		// I - PDF file
{
  pdfio_array_t	*kids;			// Pages array
  size_t	i;			// Looping var


  // Build the "Kids" array pointing to each page...
  if ((kids = pdfioArrayCreate(pdf)) == NULL)
    return (false);

  for (i = 0; i < pdf->num_pages; i ++)
    pdfioArrayAppendObj(kids, pdf->pages[i]);

  pdfioDictSetNumber(pdf->pages_obj->value.value.dict, "Count", pdf->num_pages);
  pdfioDictSetArray(pdf->pages_obj->value.value.dict, "Kids", kids);

  // Write the Pages object...
  return (pdfioObjClose(pdf->pages_obj));
}


//
// 'write_trailer()' - Write the PDF catalog object, xref table, and trailer.
//

static bool				// O - `true` on success, `false` on failure
write_trailer(pdfio_file_t *pdf)	// I - PDF file
{
  bool		ret = true;		// Return value
  off_t		xref_offset;		// Offset to xref table
  size_t	i;			// Looping var
  pdfio_obj_t	*obj;			// Current object


  // Write the xref table...
  xref_offset = _pdfioFileTell(pdf);

  if (strcmp(pdf->version, "1.5") >= 0 && !pdf->output_cb)
  {
    // Write a cross-reference stream...
    pdfio_dict_t	*xref_dict;	// Object dictionary
    pdfio_array_t	*w_array;	// W array
    pdfio_obj_t		*xref_obj;	// Object
    pdfio_stream_t	*xref_st;	// Stream
    int			offsize;	// Size of object offsets
    unsigned char	buffer[10];	// Buffer entry
    pdfio_encryption_t	encryption;	// PDF encryption mode

    // Disable encryption while we write the xref stream...
    encryption      = pdf->encryption;
    pdf->encryption = PDFIO_ENCRYPTION_NONE;

    // Figure out how many bytes are needed for the object numbers
    if (xref_offset < 0xff)
      offsize = 1;
    else if (xref_offset < 0xffff)
      offsize = 2;
    else if (xref_offset < 0xffffff)
      offsize = 3;
    else if (xref_offset < 0xffffffff)
      offsize = 4;
    else if (xref_offset < 0xffffffffff)
      offsize = 5;
    else if (xref_offset < 0xffffffffffff)
      offsize = 6;
    else if (xref_offset < 0xffffffffffffff)
      offsize = 7;
    else
      offsize = 8;

    // Create the object...
    if ((w_array = pdfioArrayCreate(pdf)) == NULL)
    {
      _pdfioFileError(pdf, "Unable to write cross-reference table.");
      ret = false;
      goto done;
    }

    pdfioArrayAppendNumber(w_array, 1);
    pdfioArrayAppendNumber(w_array, offsize);
    pdfioArrayAppendNumber(w_array, 1);

    if ((xref_dict = pdfioDictCreate(pdf)) == NULL)
    {
      _pdfioFileError(pdf, "Unable to write cross-reference table.");
      ret = false;
      goto done;
    }

    pdfioDictSetName(xref_dict, "Type", "XRef");
    pdfioDictSetNumber(xref_dict, "Size", pdf->num_objs + 2);
    pdfioDictSetArray(xref_dict, "W", w_array);
    pdfioDictSetName(xref_dict, "Filter", "FlateDecode");
    pdfioDictSetObj(xref_dict, "Info", pdf->info_obj);
    pdfioDictSetObj(xref_dict, "Root", pdf->root_obj);

    if (pdf->encrypt_obj)
      pdfioDictSetObj(xref_dict, "Encrypt", pdf->encrypt_obj);

    if (pdf->id_array)
      pdfioDictSetArray(xref_dict, "ID", pdf->id_array);

    if ((xref_obj = pdfioFileCreateObj(pdf, xref_dict)) == NULL)
    {
      _pdfioFileError(pdf, "Unable to write cross-reference table.");
      ret = false;
      goto done;
    }

    if ((xref_st = pdfioObjCreateStream(xref_obj, PDFIO_FILTER_FLATE)) == NULL)
    {
      _pdfioFileError(pdf, "Unable to write cross-reference table.");
      ret = false;
      goto done;
    }

    // Write the "free" 0 object...
    memset(buffer, 0, sizeof(buffer));
    pdfioStreamWrite(xref_st, buffer, offsize + 2);

    // Then write the "allocated" objects...
    buffer[0] = 1;

    for (i = 0; i < pdf->num_objs; i ++)
    {
      obj = pdf->objs[i];		// Current object

      switch (offsize)
      {
        case 1 :
            buffer[1] = obj->offset & 255;
            break;
        case 2 :
            buffer[1] = (obj->offset >> 8) & 255;
            buffer[2] = obj->offset & 255;
            break;
        case 3 :
            buffer[1] = (obj->offset >> 16) & 255;
            buffer[2] = (obj->offset >> 8) & 255;
            buffer[3] = obj->offset & 255;
            break;
        case 4 :
            buffer[1] = (obj->offset >> 24) & 255;
            buffer[2] = (obj->offset >> 16) & 255;
            buffer[3] = (obj->offset >> 8) & 255;
            buffer[4] = obj->offset & 255;
            break;
        case 5 :
            buffer[1] = (obj->offset >> 32) & 255;
            buffer[2] = (obj->offset >> 24) & 255;
            buffer[3] = (obj->offset >> 16) & 255;
            buffer[4] = (obj->offset >> 8) & 255;
            buffer[5] = obj->offset & 255;
            break;
        case 6 :
            buffer[1] = (obj->offset >> 40) & 255;
            buffer[2] = (obj->offset >> 32) & 255;
            buffer[3] = (obj->offset >> 24) & 255;
            buffer[4] = (obj->offset >> 16) & 255;
            buffer[5] = (obj->offset >> 8) & 255;
            buffer[6] = obj->offset & 255;
            break;
        case 7 :
            buffer[1] = (obj->offset >> 48) & 255;
            buffer[2] = (obj->offset >> 40) & 255;
            buffer[3] = (obj->offset >> 32) & 255;
            buffer[4] = (obj->offset >> 24) & 255;
            buffer[5] = (obj->offset >> 16) & 255;
            buffer[6] = (obj->offset >> 8) & 255;
            buffer[7] = obj->offset & 255;
            break;
        default :
            buffer[1] = (obj->offset >> 56) & 255;
            buffer[2] = (obj->offset >> 48) & 255;
            buffer[3] = (obj->offset >> 40) & 255;
            buffer[4] = (obj->offset >> 32) & 255;
            buffer[5] = (obj->offset >> 24) & 255;
            buffer[6] = (obj->offset >> 16) & 255;
            buffer[7] = (obj->offset >> 8) & 255;
            buffer[8] = obj->offset & 255;
            break;
      }

      if (!pdfioStreamWrite(xref_st, buffer, offsize + 2))
      {
	_pdfioFileError(pdf, "Unable to write cross-reference table.");
	ret = false;
	goto done;
      }
    }

    pdfioStreamClose(xref_st);

    pdf->encryption = encryption;
  }
  else
  {
    // Write a cross-reference table...
    if (!_pdfioFilePrintf(pdf, "xref\n0 %lu \n0000000000 65535 f \n", (unsigned long)pdf->num_objs + 1))
    {
      _pdfioFileError(pdf, "Unable to write cross-reference table.");
      ret = false;
      goto done;
    }

    for (i = 0; i < pdf->num_objs; i ++)
    {
      obj = pdf->objs[i];		// Current object

      if (!_pdfioFilePrintf(pdf, "%010lu %05u n \n", (unsigned long)obj->offset, obj->generation))
      {
	_pdfioFileError(pdf, "Unable to write cross-reference table.");
	ret = false;
	goto done;
      }
    }

    // Write the trailer...
    if (!_pdfioFilePuts(pdf, "trailer\n"))
    {
      _pdfioFileError(pdf, "Unable to write trailer.");
      ret = false;
      goto done;
    }

    if ((pdf->trailer_dict = pdfioDictCreate(pdf)) == NULL)
    {
      _pdfioFileError(pdf, "Unable to create trailer.");
      ret = false;
      goto done;
    }

    if (pdf->encrypt_obj)
      pdfioDictSetObj(pdf->trailer_dict, "Encrypt", pdf->encrypt_obj);
    if (pdf->id_array)
      pdfioDictSetArray(pdf->trailer_dict, "ID", pdf->id_array);
    pdfioDictSetObj(pdf->trailer_dict, "Info", pdf->info_obj);
    pdfioDictSetObj(pdf->trailer_dict, "Root", pdf->root_obj);
    pdfioDictSetNumber(pdf->trailer_dict, "Size", pdf->num_objs + 1);

    if (!_pdfioDictWrite(pdf->trailer_dict, NULL, NULL))
    {
      _pdfioFileError(pdf, "Unable to write trailer.");
      ret = false;
      goto done;
    }
  }

  if (!_pdfioFilePrintf(pdf, "\nstartxref\n%lu\n%%EOF\n", (unsigned long)xref_offset))
  {
    _pdfioFileError(pdf, "Unable to write xref offset.");
    ret = false;
  }

  done:

  return (ret);
}
