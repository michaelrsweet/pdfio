Frequently Asked Questions
==========================

Why Don't You Support Writing a PDF File with Encryption?
---------------------------------------------------------

PDF encryption offers very little protection:

- PDF encryption keys are reused and derived from the user password (padded
  with a standard base string) and the object numbers in the file.
- RC4 encryption (40- and 128-bit) was broken years ago.
- AES encryption (128- and 256-bit) is better, but PDF uses Cipher Block
  Chaining (CBC) which enables attacks that allow the original encryption key
  to be recovered.

In addition, PDF usage controls (no print, no copy, etc.) are tied to this
encryption, making them trivial to bypass.
