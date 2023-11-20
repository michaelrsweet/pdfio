Changes in PDFio
================


v1.1.4 (Month DD, YYYY)
-----------------------

- Fixed detection of encrypted strings that are too short (Issue #52)
- Fixed a TrueType CMAP decoding bug.
- Fixed a text rendering issue for Asian text.
- Added a ToUnicode map for Unicode text to support text copying.


v1.1.3 (November 15, 2023)
--------------------------

- Fixed Unicode font support (Issue #16)
- Fixed missing initializer for 40-bit RC4 encryption (Issue #51)


v1.1.2 (October 10, 2023)
-------------------------

- Updated `pdfioContentSetDashPattern` to support setting a solid (0 length)
  dash pattern (Issue #41)
- Fixed an issue with broken PDF files containing extra CR and/or LF separators
  after the object stream token (Issue #40)
- Fixed an issue with PDF files produced by Crystal Reports (Issue #45)
- Fixed an issue with PDF files produced by Microsoft Reporting Services
  (Issue #46)
- Fixed support for compound filters where the filter array consists of a
  single named filter (Issue #47)
- Fixed builds on Windows - needed windows.h header for temporary files
  (Issue #48)


v1.1.1 (March 20, 2023)
-----------------------

- CVE-2023-28428: Fixed a potential denial-of-service with corrupt PDF files.
- Fixed a few build issues.


v1.1.0 (February 6, 2023)
-------------------------

- CVE-2023-24808: Fixed a potential denial-of-service with corrupt PDF files.
- Added `pdfioFileCreateTemporary` function (Issue #29)
- Added `pdfioDictIterateKeys` function (Issue #31)
- Added `pdfioContentPathEnd` function.
- Added protection against opening multiple streams in the same file at the
  same time.
- Documentation updates (Issue #37)
- Fixed "install-shared" target (Issue #32)
- Fixed `pdfioFileGet...` metadata APIs (Issue #33)
- Fixed `pdfioContentMatrixRotate` function.


v1.0.1 (March 2, 2022)
----------------------

- Added missing `pdfioPageGetNumStreams` and `pdfioPageOpenStream` functions.
- Added demo pdfiototext utility.
- Fixed bug in `pdfioStreamGetToken`.


v1.0.0 (December 14, 2021)
--------------------------

- First stable release.


v1.0rc1 (November 30, 2021)
---------------------------

- Fixed a few stack/buffer overflow bugs discovered via fuzzing.


v1.0b2 (November 7, 2021)
-------------------------

- Added `pdfioFileCreateOutput` API to support streaming output of PDF
  (Issue #21)
- Fixed `all-shared` target (Issue #22)
- Fixed memory leaks (Issue #23)
- Updated `pdfioContentSetDashPattern` to accept `double` values (Issue #25)
- Added support for reading and writing encrypted PDFs (Issue #26)
- Fixed some issues identified by a Coverity scan.


v1.0b1 (August 30, 2021)
------------------------

- Initial release
