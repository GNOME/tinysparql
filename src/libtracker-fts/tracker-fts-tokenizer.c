/*
 * Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
 *
 * Author: Carlos Garnacho <carlos@lanedo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

/* FTS3/4 Tokenizer using TrackerParser */

#include <assert.h>
#include <string.h>
#include "tracker-fts-tokenizer.h"
#include "tracker-fts-config.h"
#include "tracker-parser.h"
#include "fts3_tokenizer.h"

typedef struct TrackerTokenizer TrackerTokenizer;
typedef struct TrackerCursor TrackerCursor;

struct TrackerTokenizer {
  sqlite3_tokenizer base;
  TrackerLanguage *language;
  int max_word_length;
  int max_words;
  gboolean enable_stemmer;
  gboolean enable_unaccent;
  gboolean ignore_numbers;
  gboolean ignore_stop_words;
};

struct TrackerCursor {
  sqlite3_tokenizer_cursor base;

  TrackerTokenizer *tokenizer;
  TrackerParser *parser;
  guint n_words;
};

/*
** Create a new tokenizer instance.
*/
static int trackerCreate(
  int argc,                            /* Number of entries in argv[] */
  const char * const *argv,            /* Tokenizer creation arguments */
  sqlite3_tokenizer **ppTokenizer      /* OUT: Created tokenizer */
){
  TrackerTokenizer *p;
  TrackerFTSConfig *config;

  p = (TrackerTokenizer *)sqlite3_malloc(sizeof(TrackerTokenizer));
  if( !p ){
    return SQLITE_NOMEM;
  }
  memset(p, 0, sizeof(TrackerTokenizer));
  p->language = tracker_language_new (NULL);

  config = tracker_fts_config_new ();

  p->max_word_length = tracker_fts_config_get_max_word_length (config);
  p->enable_stemmer = tracker_fts_config_get_enable_stemmer (config);
  p->enable_unaccent = tracker_fts_config_get_enable_unaccent (config);
  p->ignore_numbers = tracker_fts_config_get_ignore_numbers (config);

  /* disable stop words if TRACKER_FTS_STOP_WORDS is set to 0 - used by tests
   *  otherwise, get value from the conf file */
  p->ignore_stop_words = (g_strcmp0 (g_getenv ("TRACKER_FTS_STOP_WORDS"), "0") == 0 ?
                          FALSE : tracker_fts_config_get_ignore_stop_words (config));

  p->max_words = tracker_fts_config_get_max_words_to_index (config);

  g_object_unref (config);

  *ppTokenizer = (sqlite3_tokenizer *)p;

  return SQLITE_OK;
}

/*
** Destroy a tokenizer
*/
static int trackerDestroy(sqlite3_tokenizer *pTokenizer){
  TrackerTokenizer *p = (TrackerTokenizer *)pTokenizer;
  g_object_unref (p->language);
  sqlite3_free(p);
  return SQLITE_OK;
}

/*
** Prepare to begin tokenizing a particular string.  The input
** string to be tokenized is pInput[0..nBytes-1].  A cursor
** used to incrementally tokenize this string is returned in 
** *ppCursor.
*/
static int trackerOpen(
  sqlite3_tokenizer *pTokenizer,         /* The tokenizer */
  const char *zInput,                    /* Input string */
  int nInput,                            /* Length of zInput in bytes */
  sqlite3_tokenizer_cursor **ppCursor    /* OUT: Tokenization cursor */
){
  TrackerTokenizer *p = (TrackerTokenizer *)pTokenizer;
  TrackerParser *parser;
  TrackerCursor *pCsr;

  if ( nInput<0 ){
    nInput = strlen(zInput);
  }

  parser = tracker_parser_new (p->language);
  tracker_parser_reset (parser, zInput, nInput,
			p->max_word_length,
			p->enable_stemmer,
			p->enable_unaccent,
			p->ignore_stop_words,
			TRUE,
			p->ignore_numbers);

  pCsr = (TrackerCursor *)sqlite3_malloc(sizeof(TrackerCursor));
  memset(pCsr, 0, sizeof(TrackerCursor));
  pCsr->tokenizer = p;
  pCsr->parser = parser;

  *ppCursor = (sqlite3_tokenizer_cursor *)pCsr;
  return SQLITE_OK;
}

/*
** Close a tokenization cursor.
*/
static int trackerClose(sqlite3_tokenizer_cursor *pCursor){
  TrackerCursor *pCsr = (TrackerCursor *)pCursor;

  tracker_parser_free (pCsr->parser);
  sqlite3_free(pCsr);
  return SQLITE_OK;
}

/*
** Extract the next token from a tokenization cursor.
*/
static int trackerNext(
  sqlite3_tokenizer_cursor *pCursor,  /* Cursor returned by simpleOpen */
  const char **ppToken,               /* OUT: *ppToken is the token text */
  int *pnBytes,                       /* OUT: Number of bytes in token */
  int *piStartOffset,                 /* OUT: Starting offset of token */
  int *piEndOffset,                   /* OUT: Ending offset of token */
  int *piPosition                     /* OUT: Position integer of token */
){
  TrackerCursor *cursor = (TrackerCursor *) pCursor;
  TrackerTokenizer *p;
  const gchar *pToken;
  gboolean stop_word;
  int pos, start, end, len;

  p  = cursor->tokenizer;

  if (cursor->n_words > p->max_words){
    return SQLITE_DONE;
  }

  do {
    pToken = tracker_parser_next (cursor->parser,
				  &pos,
				  &start, &end,
				  &stop_word,
				  &len);

    if (!pToken){
      return SQLITE_DONE;
    }
  } while (stop_word && p->ignore_stop_words);

  *ppToken = pToken;
  *piStartOffset = start;
  *piEndOffset = end;
  *piPosition = pos;
  *pnBytes = len;

  cursor->n_words++;

  return SQLITE_OK;
}

/*
** The set of routines that implement the simple tokenizer
*/
static const sqlite3_tokenizer_module trackerTokenizerModule = {
  0,                           /* iVersion */
  trackerCreate,               /* xCreate  */
  trackerDestroy,              /* xDestroy */
  trackerOpen,                 /* xOpen    */
  trackerClose,                /* xClose   */
  trackerNext,                 /* xNext    */
};

/*
** Set *ppModule to point at the implementation of the tracker tokenizer.
*/
gboolean tracker_tokenizer_initialize (sqlite3 *db) {
  const sqlite3_tokenizer_module *pTokenizer;
  int rc = SQLITE_OK;
  sqlite3_stmt *stmt;

  pTokenizer = &trackerTokenizerModule;
  rc = sqlite3_prepare_v2(db, "SELECT fts3_tokenizer(?, ?)",
                          -1, &stmt, 0);

  if (rc != SQLITE_OK) {
	  return FALSE;
  }

  sqlite3_bind_text(stmt, 1, "TrackerTokenizer", -1, SQLITE_STATIC);
  sqlite3_bind_blob(stmt, 2, &pTokenizer, sizeof(pTokenizer), SQLITE_STATIC);
  sqlite3_step(stmt);
  rc = sqlite3_finalize(stmt);

  return (rc == SQLITE_OK);
}
