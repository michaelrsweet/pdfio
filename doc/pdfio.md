Introduction
============

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of pdfio are:

- Read any PDF file with or without encryption or linearization
- Write PDF files without encryption or linearization
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

PDFio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.
