---
title: Markdown to PDF Converter Example
...

Markdown to PDF Converter Example
=================================

The `md2pdf` example program reads a markdown file and formats the content onto
pages in a PDF file.  It demonstrates how to:

- Embed base and TrueType fonts,
- Format text,
- Embed JPEG and PNG images,
- Add headers and footers, and
- (Future) Add hyperlinks.


Source Files
------------

The `md2pdf` program is organized into three source files: `md2pdf.c` which
contains the code to format the markdown content and `mmd.h` and `mmd.c` which
load the markdown content, which come from the [Miniature Markdown Library][MMD]
project which is provided under the terms of the Apache License v2.0.
![Apache License v2.0](apache-badge.png)

[MMD]: https://www.msweet.org/mmd/
