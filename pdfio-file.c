//
// PDF file functions for PDFio.
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

static pdfio_obj_t	*add_obj(pdfio_file_t *pdf, size_t number, unsigned short generation, off_t offset);
static int		compare_objmaps(_pdfio_objmap_t *a, _pdfio_objmap_t *b);
static int		compare_objs(pdfio_obj_t **a, pdfio_obj_t **b);
static bool		load_obj_stream(pdfio_obj_t *obj);
static bool		load_pages(pdfio_file_t *pdf, pdfio_obj_t *obj);
static bool		load_xref(pdfio_file_t *pdf, off_t xref_offset);
static bool		write_catalog(pdfio_file_t *pdf);
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

    if (pdfioObjClose(pdf->info))
      if (write_pages(pdf))
	if (write_catalog(pdf))
	  if (write_trailer(pdf))
	    ret = _pdfioFileFlush(pdf);
  }

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

pdfio_file_t *				// O - PDF file or `NULL` on error
pdfioFileCreate(
    const char       *filename,		// I - Filename
    const char       *version,		// I - PDF version number or `NULL` for default (2.0)
    pdfio_rect_t     *media_box,	// I - Default MediaBox for pages
    pdfio_rect_t     *crop_box,		// I - Default CropBox for pages
    pdfio_error_cb_t error_cb,		// I - Error callback or `NULL` for default
    void             *error_data)	// I - Error callback data, if any
{
  pdfio_file_t	*pdf;			// PDF file
  pdfio_dict_t	*dict;			// Dictionary for pages object
  pdfio_dict_t	*info_dict;		// Dictionary for information object


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
  pdf->bufptr     = pdf->buffer;
  pdf->bufend     = pdf->buffer + sizeof(pdf->buffer);

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

  // Create the pages object...
  if ((dict = pdfioDictCreate(pdf)) == NULL)
  {
    pdfioFileClose(pdf);
    unlink(filename);
    return (NULL);
  }

  pdfioDictSetName(dict, "Type", "Pages");

  if ((pdf->pages_root = pdfioFileCreateObj(pdf, dict)) == NULL)
  {
    pdfioFileClose(pdf);
    unlink(filename);
    return (NULL);
  }

  // Create the info object...
  if ((info_dict = pdfioDictCreate(pdf)) == NULL)
  {
    pdfioFileClose(pdf);
    unlink(filename);
    return (NULL);
  }

  pdfioDictSetDate(info_dict, "CreationDate", time(NULL));
  pdfioDictSetString(info_dict, "Producer", "pdfio/" PDFIO_VERSION);

  if ((pdf->info = pdfioFileCreateObj(pdf, info_dict)) == NULL)
  {
    pdfioFileClose(pdf);
    unlink(filename);
    return (NULL);
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

  pdfioDictSetObj(dict, "Parent", pdf->pages_root);

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
  pdfio_obj_t	key,			// Search key
		*keyptr,		// Pointer to key
		**match;		// Pointer to match


  if (pdf->num_objs > 0)
  {
    key.number = number;
    keyptr     = &key;
    match      = (pdfio_obj_t **)bsearch(&keyptr, pdf->objs, pdf->num_objs, sizeof(pdfio_obj_t *), (int (*)(const void *, const void *))compare_objs);

    return (match ? *match : NULL);
  }

  return (NULL);
}


//
// 'pdfioFileGetAuthor()' - Get the author for a PDF file.
//

const char *				// O - Author or `NULL` for none
pdfioFileGetAuthor(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Author") : NULL);
}


//
// 'pdfioFileGetCreationDate()' - Get the creation date for a PDF file.
//

time_t					// O - Creation date or `0` for none
pdfioFileGetCreationDate(
    pdfio_file_t *pdf)			// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetDate(pdf->info->value.value.dict, "CreationDate") : 0);
}


//
// 'pdfioFileGetCreator()' - Get the creator string for a PDF file.
//

const char *				// O - Creator string or `NULL` for none
pdfioFileGetCreator(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Creator") : NULL);
}


//
// 'pdfioFileGetID()' - Get the PDF file's ID strings.
//

pdfio_array_t *				// O - Array with binary strings
pdfioFileGetID(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetArray(pdf->trailer, "ID") : NULL);
}


//
// 'pdfioFileGetKeywords()' - Get the keywords for a PDF file.
//

const char *				// O - Keywords string or `NULL` for none
pdfioFileGetKeywords(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Keywords") : NULL);
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
// 'pdfioFileGetProducer()' - Get the producer string for a PDF file.
//

const char *				// O - Producer string or `NULL` for none
pdfioFileGetProducer(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Producer") : NULL);
}


//
// 'pdfioFileGetSubject()' - Get the subject for a PDF file.
//

const char *				// O - Subject or `NULL` for none
pdfioFileGetSubject(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Subject") : NULL);
}


//
// 'pdfioFileGetTitle()' - Get the title for a PDF file.
//

const char *				// O - Title or `NULL` for none
pdfioFileGetTitle(pdfio_file_t *pdf)	// I - PDF file
{
  return (pdf && pdf->info ? pdfioDictGetString(pdf->info->value.value.dict, "Title") : NULL);
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
  if (_pdfioFileSeek(pdf, -32, SEEK_END) < 0)
  {
    _pdfioFileError(pdf, "Unable to read startxref data.");
    goto error;
  }

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
// 'pdfioFileSetAuthor()' - Set the author for a PDF file.
//

void
pdfioFileSetAuthor(pdfio_file_t *pdf,	// I - PDF file
                   const char   *value)	// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetString(pdf->info->value.value.dict, "Author", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetCreationDate()' - Set the creation date for a PDF file.
//

void
pdfioFileSetCreationDate(
    pdfio_file_t *pdf,			// I - PDF file
    time_t       value)			// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetDate(pdf->info->value.value.dict, "CreationDate", value);
}


//
// 'pdfioFileSetCreator()' - Set the creator string for a PDF file.
//

void
pdfioFileSetCreator(pdfio_file_t *pdf,	// I - PDF file
                    const char   *value)// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetString(pdf->info->value.value.dict, "Creator", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetKeywords()' - Set the keywords string for a PDF file.
//

void
pdfioFileSetKeywords(
    pdfio_file_t *pdf,			// I - PDF file
    const char   *value)		// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetString(pdf->info->value.value.dict, "Keywords", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetSubject()' - Set the subject for a PDF file.
//

void
pdfioFileSetSubject(pdfio_file_t *pdf,	// I - PDF file
                   const char    *value)// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetString(pdf->info->value.value.dict, "Subject", pdfioStringCreate(pdf, value));
}


//
// 'pdfioFileSetTitle()' - Set the title for a PDF file.
//

void
pdfioFileSetTitle(pdfio_file_t *pdf,	// I - PDF file
                  const char   *value)	// I - Value
{
  if (pdf && pdf->info)
    pdfioDictSetString(pdf->info->value.value.dict, "Title", pdfioStringCreate(pdf, value));
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

  obj->pdf        = pdf;
  obj->number     = number;
  obj->generation = generation;
  obj->offset     = offset;

  PDFIO_DEBUG("add_obj: obj=%p, ->pdf=%p, ->number=%lu\n", obj, pdf, (unsigned long)obj->number);

  // Re-sort object array as needed...
  if (pdf->num_objs > 1 && pdf->objs[pdf->num_objs - 2]->number > number)
    qsort(pdf->objs, pdf->num_objs, sizeof(pdfio_obj_t *), (int (*)(const void *, const void *))compare_objs);

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
// 'compare_objs()' - Compare the object numbers of two objects.
//

static int				// O - Result of comparison
compare_objs(pdfio_obj_t **a,		// I - First object
             pdfio_obj_t **b)		// I - Second object
{
  if ((*a)->number < (*b)->number)
    return (-1);
  else if ((*a)->number == (*b)->number)
    return (0);
  else
    return (1);
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
  size_t		cur_obj,	// Current object
			num_objs = 0;	// Number of objects
  pdfio_obj_t		*objs[1000];	// Objects


  // Open the object stream...
  if ((st = pdfioObjOpenStream(obj, true)) == NULL)
  {
    _pdfioFileError(obj->pdf, "Unable to open compressed object stream %lu.", (unsigned long)obj->number);
    return (false);
  }

  _pdfioTokenInit(&tb, obj->pdf, (_pdfio_tconsume_cb_t)pdfioStreamConsume, (_pdfio_tpeek_cb_t)pdfioStreamPeek, st);

  // Read the object numbers from the beginning of the stream...
  while (_pdfioTokenGet(&tb, buffer, sizeof(buffer)))
  {
    // Stop if this isn't an object number...
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
    objs[num_objs ++] = add_obj(obj->pdf, (size_t)strtoimax(buffer, NULL, 10), 0, 0);

    // Skip offset
    _pdfioTokenGet(&tb, buffer, sizeof(buffer));
  }

  if (!buffer[0])
  {
    pdfioStreamClose(st);
    return (false);
  }

  _pdfioTokenPush(&tb, buffer);

  // Read the objects themselves...
  for (cur_obj = 0; cur_obj < num_objs; cur_obj ++)
  {
    if (!_pdfioValueRead(obj->pdf, &tb, &(objs[cur_obj]->value)))
    {
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
           pdfio_obj_t  *obj)		// I - Page object
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

    for (i = 0, num_kids = pdfioArrayGetSize(kids); i < num_kids; i ++)
    {
      if (!load_pages(pdf, pdfioArrayGetObj(kids, i)))
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
load_xref(pdfio_file_t *pdf,		// I - PDF file
          off_t        xref_offset)	// I - Offset to xref
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

    PDFIO_DEBUG("load_xref: xref_offset=%lu, line='%s'\n", (unsigned long)xref_offset, line);

    if (isdigit(line[0] & 255) && strlen(line) > 4 && !strcmp(line + strlen(line) - 4, " obj"))
    {
      // Cross-reference stream
      pdfio_obj_t	*obj;		// Object
      size_t		i;		// Looping var
      pdfio_array_t	*index_array;	// Index array
      pdfio_array_t	*w_array;	// W array
      size_t		w[3];		// Size of each cross-reference field
      size_t		w_2,		// Offset to second field
			w_3;		// Offset to third field
      size_t		w_total;	// Total length
      pdfio_stream_t	*st;		// Stream
      unsigned char	buffer[32];	// Read buffer
      size_t		num_sobjs = 0,	// Number of object streams
			sobjs[1000];	// Object streams to load

      if ((number = strtoimax(line, &ptr, 10)) < 1)
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      if ((generation = (int)strtol(ptr, &ptr, 10)) < 0 || generation > 65535)
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      while (isspace(*ptr & 255))
	ptr ++;

      if (strcmp(ptr, "obj"))
      {
	_pdfioFileError(pdf, "Bad xref table header '%s'.", line);
	return (false);
      }

      PDFIO_DEBUG("load_xref: Loading object %lu %u.\n", (unsigned long)number, (unsigned)generation);

      if ((obj = add_obj(pdf, (size_t)number, (unsigned short)generation, xref_offset)) == NULL)
      {
        _pdfioFileError(pdf, "Unable to allocate memory for object.");
        return (false);
      }

      _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);

      if (!_pdfioValueRead(pdf, &tb, &trailer))
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

      _pdfioTokenFlush(&tb);

      obj->stream_offset = _pdfioFileTell(pdf);

      if ((index_array = pdfioDictGetArray(trailer.value.dict, "Index")) != NULL)
      {
        if (index_array->num_values > 2)
        {
          // TODO: Support Index array with multiple values in xref streams
	  _pdfioFileError(pdf, "Multiple indices not supported in cross-reference stream.");
	  return (false);
        }

        number = (intmax_t)pdfioArrayGetNumber(index_array, 0);
      }
      else
        number = 0;

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

      if (w[1] == 0 || w[2] > 2 || w_total > sizeof(buffer))
      {
	_pdfioFileError(pdf, "Cross-reference stream has invalid W key.");
	return (false);
      }

      if ((st = pdfioObjOpenStream(obj, true)) == NULL)
      {
	_pdfioFileError(pdf, "Unable to open cross-reference stream.");
	return (false);
      }

      while (pdfioStreamRead(st, buffer, w_total) > 0)
      {
        PDFIO_DEBUG("load_xref: %02X%02X%02X%02X%02X\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);

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

        for (i = 1, offset = buffer[w_2]; i < w[1]; i ++)
          offset = (offset << 8) | buffer[w_2 + i];

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
        }

	// Create a placeholder for the object in memory...
	if (pdfioFileFindObj(pdf, (size_t)number))
	{
	  number ++;
	  continue;			// Don't replace newer object...
	}

        if (w[0] > 0 && buffer[0] == 2)
        {
          // Object streams need to be loaded into memory...
          if ((obj = pdfioFileFindObj(pdf, (size_t)offset)) != NULL)
	  {
	    // Load it now...
	    if (!load_obj_stream(obj))
	      return (false);
	  }
	  else
	  {
	    // Add it to the list of objects to load later...
	    for (i = 0; i < num_sobjs; i ++)
	    {
	      if (sobjs[i] == (size_t)offset)
	        break;
	    }

	    if (i >= num_sobjs && num_sobjs < (sizeof(sobjs) / sizeof(sobjs[0])))
	      sobjs[num_sobjs ++] = (size_t)offset;
	  }
        }
	else if (!add_obj(pdf, (size_t)number, (unsigned short)generation, offset))
	  return (false);

        number ++;
      }

      pdfioStreamClose(st);

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
    else if (!strcmp(line, "xref"))
    {
      // Read the xref tables
      while (_pdfioFileGets(pdf, line, sizeof(line)))
      {
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
	  if (pdfioFileFindObj(pdf, (size_t)number))
	    continue;			// Don't replace newer object...

	  if (!add_obj(pdf, (size_t)number, (unsigned short)generation, offset))
	    return (false);
	}
      }

      if (strcmp(line, "trailer"))
      {
	_pdfioFileError(pdf, "Missing trailer.");
	return (false);
      }

      _pdfioTokenInit(&tb, pdf, (_pdfio_tconsume_cb_t)_pdfioFileConsume, (_pdfio_tpeek_cb_t)_pdfioFilePeek, pdf);

      if (!_pdfioValueRead(pdf, &tb, &trailer))
      {
	_pdfioFileError(pdf, "Unable to read trailer dictionary.");
	return (false);
      }
      else if (trailer.type != PDFIO_VALTYPE_DICT)
      {
	_pdfioFileError(pdf, "Trailer is not a dictionary.");
	return (false);
      }

      _pdfioTokenFlush(&tb);
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

    if (!pdf->trailer)
    {
      // Save the trailer dictionary and grab the root (catalog) and info
      // objects...
      pdf->trailer = trailer.value.dict;
    }

    if ((xref_offset = (off_t)pdfioDictGetNumber(trailer.value.dict, "Prev")) <= 0)
      done = true;
  }

  // Once we have all of the xref tables loaded, get the important objects and
  // build the pages array...
  if ((pdf->root = pdfioDictGetObj(pdf->trailer, "Root")) == NULL)
  {
    _pdfioFileError(pdf, "Missing Root object.");
    return (false);
  }

  PDFIO_DEBUG("load_xref: Root=%p(%lu)\n", pdf->root, (unsigned long)pdf->root->number);

  pdf->info     = pdfioDictGetObj(pdf->trailer, "Info");
  pdf->encrypt  = pdfioDictGetObj(pdf->trailer, "Encrypt");
  pdf->id_array = pdfioDictGetArray(pdf->trailer, "ID");

  return (load_pages(pdf, pdfioDictGetObj(pdfioObjGetDict(pdf->root), "Pages")));
}


//
// 'write_catalog()' - Write the PDF root object/catalog.
//

static bool				// O - `true` on success, `false` on failure
write_catalog(pdfio_file_t *pdf)	// I - PDF file
{
  pdfio_dict_t	*dict;			// Dictionary for catalog...


  if ((dict = pdfioDictCreate(pdf)) == NULL)
    return (false);

  pdfioDictSetName(dict, "Type", "Catalog");
  pdfioDictSetObj(dict, "Pages", pdf->pages_root);
  // TODO: Add support for all of the root object dictionary keys

  if ((pdf->root = pdfioFileCreateObj(pdf, dict)) == NULL)
    return (false);
  else
    return (pdfioObjClose(pdf->root));
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

  pdfioDictSetNumber(pdf->pages_root->value.value.dict, "Count", pdf->num_pages);
  pdfioDictSetArray(pdf->pages_root->value.value.dict, "Kids", kids);

  // Write the Pages object...
  return (pdfioObjClose(pdf->pages_root));
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
  int		fd;			// File for /dev/urandom
  unsigned char	id_values[2][16];	// ID array values


  // Write the xref table...
  // TODO: Look at adding support for xref streams...
  xref_offset = _pdfioFileTell(pdf);

  if (!_pdfioFilePrintf(pdf, "xref\n0 %lu \n0000000000 65535 f \n", (unsigned long)pdf->num_objs + 1))
  {
    _pdfioFileError(pdf, "Unable to write cross-reference table.");
    ret = false;
    goto done;
  }

  for (i = 0; i < pdf->num_objs; i ++)
  {
    pdfio_obj_t	*obj = pdf->objs[i];	// Current object

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

  if ((fd = open("/dev/urandom", O_RDONLY)) >= 0)
  {
    // Load ID array with random values from /dev/urandom...
    if (read(fd, id_values[0], sizeof(id_values[0])) == (ssize_t)sizeof(id_values[0]) && read(fd, id_values[1], sizeof(id_values[1])) == (ssize_t)sizeof(id_values[1]))
    {
      pdf->id_array = pdfioArrayCreate(pdf);
      pdfioArrayAppendBinary(pdf->id_array, id_values[0], sizeof(id_values[0]));
      pdfioArrayAppendBinary(pdf->id_array, id_values[1], sizeof(id_values[1]));
    }

    close(fd);
  }

  if ((pdf->trailer = pdfioDictCreate(pdf)) == NULL)
  {
    ret = false;
    goto done;
  }

  if (pdf->encrypt)
    pdfioDictSetObj(pdf->trailer, "Encrypt", pdf->encrypt);
  if (pdf->id_array)
    pdfioDictSetArray(pdf->trailer, "ID", pdf->id_array);
  pdfioDictSetObj(pdf->trailer, "Info", pdf->info);
  pdfioDictSetObj(pdf->trailer, "Root", pdf->root);
  pdfioDictSetNumber(pdf->trailer, "Size", pdf->num_objs + 1);

  if (!_pdfioDictWrite(pdf->trailer, NULL))
  {
    _pdfioFileError(pdf, "Unable to write trailer.");
    ret = false;
    goto done;
  }

  if (!_pdfioFilePrintf(pdf, "\nstartxref\n%lu\n%%EOF\n", (unsigned long)xref_offset))
  {
    _pdfioFileError(pdf, "Unable to write xref offset.");
    ret = false;
  }

  done:

  return (ret);
}
