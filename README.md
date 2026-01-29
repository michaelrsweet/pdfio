PDFio - PDF Read/Write Library
==============================

![Version](https://img.shields.io/github/v/release/michaelrsweet/pdfio?include_prereleases)
![Apache 2.0](https://img.shields.io/github/license/michaelrsweet/pdfio)
[![Build Status](https://img.shields.io/github/actions/workflow/status/michaelrsweet/pdfio/build.yml)](https://github.com/michaelrsweet/pdfio/actions/workflows/build.yml)
[![Coverity Scan Status](https://img.shields.io/coverity/scan/23194.svg)](https://scan.coverity.com/projects/michaelrsweet-pdfio)

PDFio is a simple C library for reading and writing PDF files.  The primary
goals of PDFio are:

- Read and write any version of PDF file
- Provide access to pages, objects, and streams within a PDF file
- Support reading and writing of encrypted PDF files
- Extract or embed useful metadata (author, creator, page information, etc.)
- "Filter" PDF files, for example to extract a range of pages or to embed fonts
  that are missing from a PDF
- Provide access to objects used for each page

PDFio is *not* concerned with rendering or viewing a PDF file, although a PDF
RIP or viewer could be written using it.

PDFio is Copyright © 2021-2026 by Michael R Sweet and is licensed under the
Apache License Version 2.0 with an (optional) exception to allow linking against
GNU GPL2-only software.  See the files `LICENSE` and `NOTICE` for more
information.


Reading the Documentation
-------------------------

Initial documentation to get you started is provided in the root directory of
the PDFio sources:

- `CHANGES.md`: A list of changes for each release of PDFio.
- `CODE_OF_CONDUCT.md`: Code of conduct for the project.
- `CONTRIBUTING.md`: Guidelines for contributing to the project.
- `INSTALL.md`: Instructions for building, testing, and installing PDFio.
- `LICENSE`: The PDFio license agreement (Apache 2.0).
- `NOTICE`: Copyright notices and exceptions to the PDFio license agreement.
- `README.md`: This file.
- `SECURITY.md`: How (and when) to report security issues.

You will find the PDFio documentation in HTML, EPUB, and man formats in the
`doc` directory.

Examples can be found in the `examples` directory.

*Please read the documentation before asking questions.*
