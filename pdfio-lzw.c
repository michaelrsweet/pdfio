//
// LZW decoding functions for PDFio.
//
// This code is used to support (legacy) PDF object streams using the LZWDecode
// filter as well as when embedding (legacy) GIF images.  None of this is public
// API and we only support reading (decoding) since FlateDecode is superior in
// every way.
//
// Copyright © 2026 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#include "pdfio-private.h"


//
// Local functions...
//

static void	lzw_clear(_pdfio_lzw_t *lzw);
static int	lzw_get_code(_pdfio_lzw_t *lzw);


//
// '_pdfioLZWCreate()' - Create a LZW decompressor.
//

_pdfio_lzw_t *				// O - LZW state
_pdfioLZWCreate(int code_size,		// I - Data code size in bits (typically 8 for PDF, 2-8 for GIF)
                int early)		// I - Number of early codes
{
  _pdfio_lzw_t	*lzw;			// LZW state


  if ((lzw = (_pdfio_lzw_t *)calloc(1, sizeof(_pdfio_lzw_t))) != NULL)
  {
    lzw->def_code_size = code_size + 1;
    lzw->clear_code    = (short)(1 << code_size);
    lzw->eod_code      = lzw->clear_code + 1;
    lzw->early         = early;

    lzw_clear(lzw);
  }

  return (lzw);
}


//
// '_pdfioLZWDelete()' - Delete a LZW decompressor.
//

void
_pdfioLZWDelete(_pdfio_lzw_t *lzw)	// I - LZW state
{
  free(lzw);
}


//
// '_pdfioLZWInflate()' - Decompress pending input data.
//

bool					// O - `true` on success, `false` on error
_pdfioLZWInflate(_pdfio_lzw_t *lzw)	// I - LZW state
{
  int	cur_code,			// Current code
	in_code;			// Input code


  // Stop if we already saw the "end of data" code...
  if (lzw->saw_eod)
  {
    PDFIO_DEBUG("_pdfioLZWInflate: EOD, returning false.\n");
    lzw->error = "End of data.";
    return (false);
  }

  // Copy pending compressed data to the output buffer...
  while (lzw->stptr > lzw->stack && lzw->avail_out > 0)
  {
    *(lzw->next_out++) = *(--lzw->stptr);
    lzw->avail_out --;
    PDFIO_DEBUG("_pdfioLZWInflate: Unrolled value %d, stptr=%p(%ld), avail_out=%u\n", *(lzw->stptr), (void *)lzw->stptr, lzw->stptr - lzw->stack, (unsigned)lzw->avail_out);
  }

  // Loop as long as we have room in the output buffer and data in the input
  // buffer...
  while (lzw->avail_out > 0)
  {
    if ((in_code = lzw_get_code(lzw)) < 0)
    {
      // Out of data, stop now...
      PDFIO_DEBUG("_pdfioLZWInflate: Out of data.\n");
      break;
    }
    else if (in_code == lzw->clear_code)
    {
      // Clear the compression tables and reset...
      lzw_clear(lzw);
      PDFIO_DEBUG("_pdfioLZWInflate: Clear.\n");
      continue;
    }
    else if (in_code == lzw->eod_code)
    {
      // End of data...
      lzw->saw_eod = true;
      PDFIO_DEBUG("_pdfioLZWInflate: EOD.\n");
      break;
    }

    // If we get this far we have something to write to the output buffer and/or
    // stack...
    if (lzw->first_code == 0xffff)
    {
      // First code...
      lzw->first_code    = lzw->old_code = in_code;
      *(lzw->next_out++) = in_code;
      lzw->avail_out --;

      PDFIO_DEBUG("_pdfioLZWInflate: first_code=%d.\n", in_code);
      continue;
    }

    PDFIO_DEBUG("_pdfioLZWInflate: in_code=%d, old_code=%d.\n", in_code, lzw->old_code);

    cur_code = in_code;

    if (cur_code >= lzw->next_code)
    {
      PDFIO_DEBUG("_pdfioLZWInflate: New cur_code=%d, next_code=%d\n", cur_code, lzw->next_code);
      *(lzw->stptr++) = lzw->first_code;
      cur_code        = lzw->old_code;
    }

    while (cur_code >= lzw->clear_code)
    {
      PDFIO_DEBUG("_pdfioLZWInflate: cur_code=%d (%d,%d)\n", cur_code, lzw->table[cur_code].prefix_code, lzw->table[cur_code].suffix);

      // Protect against overflow/loops...
      if (lzw->stptr >= (lzw->stack + sizeof(lzw->stack) / sizeof(lzw->stack[0])))
      {
	PDFIO_DEBUG("_pdfioLZWInflate: Stack overflow, returning false.\n");
	lzw->error = "Output overflow.";
        return (false);
      }

      // Add this character to the output stack and move to the next character
      // in the sequence...
      *(lzw->stptr++) = lzw->table[cur_code].suffix;

      if (cur_code == lzw->table[cur_code].prefix_code)
      {
	PDFIO_DEBUG("_pdfioLZWInflate: Table loop on code %d, returning false.\n", cur_code);
	lzw->error = "Table loop detected.";
	return (false);
      }

      cur_code = lzw->table[cur_code].prefix_code;
    }

    if (lzw->stptr >= (lzw->stack + sizeof(lzw->stack) / sizeof(lzw->stack[0])))
    {
      PDFIO_DEBUG("_pdfioLZWInflate: Stack overflow, returning false.\n");
      lzw->error = "Output overflow.";
      return (false);
    }

    *(lzw->stptr++) = lzw->first_code = lzw->table[cur_code].suffix;

    if ((cur_code = lzw->next_code) < 4096)
    {
      PDFIO_DEBUG("_pdfioLZWInflate: Adding code %d (%d,%d), next_size_code=%d\n", cur_code, lzw->old_code, lzw->first_code, lzw->next_size_code);

      lzw->table[cur_code].prefix_code = lzw->old_code;
      lzw->table[cur_code].suffix      = lzw->first_code;
      lzw->next_code ++;

      if (lzw->next_code >= lzw->next_size_code && lzw->cur_code_size < 12)
      {
        lzw->cur_code_size ++;
        lzw->next_size_code = (1 << lzw->cur_code_size) - lzw->early;
        PDFIO_DEBUG("_pdfioLZWInflate: Increased code size to %u, next_size_code=%u\n", lzw->cur_code_size, lzw->next_size_code);
      }
    }

    lzw->old_code = (uint16_t)in_code;

    while (lzw->stptr > lzw->stack && lzw->avail_out > 0)
    {
      *(lzw->next_out++) = *(--lzw->stptr);
      lzw->avail_out --;
      PDFIO_DEBUG("_pdfioLZWInflate: Unrolled value %d, stptr=%p(%ld), avail_out=%u\n", *(lzw->stptr), (void *)lzw->stptr, lzw->stptr - lzw->stack, (unsigned)lzw->avail_out);
    }
  }

  PDFIO_DEBUG("_pdfioLZWInflate: Returning true, avail_in=%u, avail_out=%u.\n", (unsigned)lzw->avail_in, (unsigned)lzw->avail_out);

  return (true);
}


//
// 'lzw_clear()' - Clear the compression table.
//

static void
lzw_clear(_pdfio_lzw_t *lzw)		// I - LZW state
{
  uint16_t	i;			// Looping var


  lzw->cur_code_size  = lzw->def_code_size;
  lzw->next_code      = lzw->clear_code + 2;
  lzw->next_size_code = (1 << lzw->cur_code_size)  - lzw->early;
  lzw->first_code     = 0xffff;
  lzw->old_code       = 0xffff;

  memset(lzw->table, 0, sizeof(lzw->table));

  for (i = 0; i < lzw->clear_code; i ++)
    lzw->table[i].suffix = i;

  lzw->stptr = lzw->stack;
}


//
// 'lzw_get_code()' - Get a code from the input buffer.
//

static int				// O - Code or -1 if there is not enough data available
lzw_get_code(_pdfio_lzw_t *lzw)		// I - LZW state
{
  uint16_t	code,			// Code
		in_bit;			// Bit offset in buffer
  uint8_t	bits,			// Bits in current byte
		boff,			// Bit offset in current byte
		byte,			// Current byte
		remaining;		// Remaining bits for code
  static uint8_t mask[8] =		// Value mask
  {
    0xff, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f
  };


  // Fill input bytes as needed...
  if ((lzw->in_bit + lzw->cur_code_size) > lzw->in_bits)
  {
    uint16_t	in_used = lzw->in_bits / 8,
					// Number of input bytes
		in_offset = lzw->in_bit / 8,
					// Offset to current input
		in_add;			// Number of bytes to "read"


    if (lzw->avail_in == 0)
    {
      // No more data
      PDFIO_DEBUG("lzw_get_code: No data, returning -1.\n");
      return (-1);
    }

    if (in_offset > 0)
    {
      // Make room in the input buffer
      memmove(lzw->in_bytes, lzw->in_bytes + in_offset, in_used - in_offset);
      in_used     -= in_offset;
      lzw->in_bit &= 7;
    }

    if ((in_add = sizeof(lzw->in_bytes) - in_used) > lzw->avail_in)
      in_add = lzw->avail_in;

    memcpy(lzw->in_bytes + in_used, lzw->next_in, in_add);
    lzw->next_in  += in_add;
    lzw->avail_in -= in_add;
    lzw->in_bits  = 8 * (in_used + in_add);

    if ((lzw->in_bit + lzw->cur_code_size) > lzw->in_bits)
    {
      // Not enough data
      PDFIO_DEBUG("lzw_get_code: Not enough data, returning -1.\n");
      return (-1);
    }
  }

  PDFIO_DEBUG("lzw_get_code: in_bit=%u, in_bits=%u, in_bytes=<...%02X%02X%02X...>, cur_code_size=%u\n", lzw->in_bit, lzw->in_bits, lzw->in_bytes[lzw->in_bit / 8], lzw->in_bytes[lzw->in_bit / 8 + 1], lzw->in_bytes[lzw->in_bit / 8 + 2], lzw->cur_code_size);

  // Now extract the code from the buffer...
  for (code = 0, in_bit = lzw->in_bit, remaining = lzw->cur_code_size; remaining > 0; in_bit += bits, remaining -= bits)
  {
    // See how many bits we can extract from the current byte...
    boff = (in_bit & 7);
    byte = lzw->in_bytes[in_bit / 8];
    bits = 8 - boff;
    if (bits > remaining)
      bits = remaining;

    // Get those bits
    if (bits == 8)			// Full byte from buffer
      code = (code << 8) | byte;
    else				// Partial byte from buffer
      code = (code << bits) | ((byte >> (8 - bits - boff)) & mask[bits]);
  }

  // Save the updated position in the input buffer and return the code...
  lzw->in_bit = in_bit;

#ifdef DEBUG
  if (code >= 0x20 && code < 0x7f)
    PDFIO_DEBUG("lzw_get_code: Returning %u('%c').\n", code, code);
  else
    PDFIO_DEBUG("lzw_get_code: Returning %u.\n", code);
#endif // DEBUG

  return ((int)code);
}
