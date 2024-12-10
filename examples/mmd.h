//
// Header file for miniature markdown library.
//
//     https://www.msweet.org/mmd
//
// Copyright © 2017-2024 by Michael R Sweet.
//
// Licensed under Apache License v2.0.  See the file "LICENSE" for more
// information.
//

#ifndef MMD_H
#  define MMD_H
#  include <stdio.h>
#  include <stdbool.h>
#  ifdef __cplusplus
extern "C" {
#  endif // __cplusplus


//
// Constants...
//

enum mmd_option_e
{
  MMD_OPTION_NONE = 0x00,		// No markdown extensions
  MMD_OPTION_METADATA = 0x01,		// Jekyll metadata extension
  MMD_OPTION_TABLES = 0x02,		// Github table extension
  MMD_OPTION_TASKS = 0x04,		// Github task item extension (check boxes)
  MMD_OPTION_ALL = 0x07			// All supported markdown extensions
};
typedef unsigned mmd_option_t;

typedef enum mmd_type_e
{
  MMD_TYPE_NONE = -1,
  MMD_TYPE_DOCUMENT,			// The document root
  MMD_TYPE_METADATA,			// Document metadata
  MMD_TYPE_BLOCK_QUOTE,			// <blockquote>
  MMD_TYPE_ORDERED_LIST,		// <ol>
  MMD_TYPE_UNORDERED_LIST,		// <ul>
  MMD_TYPE_LIST_ITEM,			// <li>
  MMD_TYPE_TABLE,			// <table>
  MMD_TYPE_TABLE_HEADER,		// <thead>
  MMD_TYPE_TABLE_BODY,			// <tbody>
  MMD_TYPE_TABLE_ROW,			// <tr>
  MMD_TYPE_HEADING_1 = 10,		// <h1>
  MMD_TYPE_HEADING_2,			// <h2>
  MMD_TYPE_HEADING_3,			// <h3>
  MMD_TYPE_HEADING_4,			// <h4>
  MMD_TYPE_HEADING_5,			// <h5>
  MMD_TYPE_HEADING_6,			// <h6>
  MMD_TYPE_PARAGRAPH,			// <p>
  MMD_TYPE_CODE_BLOCK,			// <pre><code>
  MMD_TYPE_THEMATIC_BREAK,		// <hr />
  MMD_TYPE_TABLE_HEADER_CELL,		// <th>
  MMD_TYPE_TABLE_BODY_CELL_LEFT,	// <td align="left">
  MMD_TYPE_TABLE_BODY_CELL_CENTER,	// <td align="center">
  MMD_TYPE_TABLE_BODY_CELL_RIGHT,	// <td align="right">
  MMD_TYPE_NORMAL_TEXT = 100,		// Normal text
  MMD_TYPE_EMPHASIZED_TEXT,		// <em>text</em>
  MMD_TYPE_STRONG_TEXT,			// <strong>text</strong>
  MMD_TYPE_STRUCK_TEXT,			// <del>text</del>
  MMD_TYPE_LINKED_TEXT,			// <a href="link">text</a>
  MMD_TYPE_CODE_TEXT,			// <code>text</code>
  MMD_TYPE_IMAGE,			// <img src="link" />
  MMD_TYPE_HARD_BREAK,			// <br />
  MMD_TYPE_SOFT_BREAK,			// <wbr />
  MMD_TYPE_METADATA_TEXT,		// name: value
  MMD_TYPE_CHECKBOX			// [ ] or [x]
} mmd_type_t;


//
// Types...
//

typedef struct _mmd_s mmd_t;		// Markdown node
typedef size_t (*mmd_iocb_t)(void *cbdata, char *buffer, size_t bytes);
					// mmdLoadIO callback function

//
// Functions...
//

extern char         *mmdCopyAllText(mmd_t *node);
extern void         mmdFree(mmd_t *node);
extern const char   *mmdGetExtra(mmd_t *node);
extern mmd_t        *mmdGetFirstChild(mmd_t *node);
extern mmd_t        *mmdGetLastChild(mmd_t *node);
extern const char   *mmdGetMetadata(mmd_t *doc, const char *keyword);
extern mmd_t        *mmdGetNextSibling(mmd_t *node);
extern mmd_option_t mmdGetOptions(void);
extern mmd_t        *mmdGetParent(mmd_t *node);
extern mmd_t        *mmdGetPrevSibling(mmd_t *node);
extern const char   *mmdGetText(mmd_t *node);
extern mmd_type_t   mmdGetType(mmd_t *node);
extern const char   *mmdGetURL(mmd_t *node);
extern bool         mmdGetWhitespace(mmd_t *node);
extern bool         mmdIsBlock(mmd_t *node);
extern mmd_t        *mmdLoad(mmd_t *root, const char *filename);
extern mmd_t        *mmdLoadFile(mmd_t *root, FILE *fp);
extern mmd_t        *mmdLoadIO(mmd_t *root, mmd_iocb_t cb, void *cbdata);
extern mmd_t        *mmdLoadString(mmd_t *root, const char *s);
extern void         mmdSetOptions(mmd_option_t options);


#  ifdef __cplusplus
}
#  endif // __cplusplus
#endif // !MMD_H
