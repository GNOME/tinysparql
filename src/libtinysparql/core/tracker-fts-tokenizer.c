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

/* FTS5 Tokenizer using TrackerParser */

#include "config.h"

#include <assert.h>
#include <string.h>

#include <tracker-common.h>

#include "tracker-data-manager.h"
#include "tracker-ontologies.h"

#include "tracker-fts-tokenizer.h"

typedef struct TrackerTokenizerData TrackerTokenizerData;
typedef struct TrackerTokenizer TrackerTokenizer;
typedef struct TrackerTokenizerFunctionData TrackerTokenizerFunctionData;

struct TrackerTokenizerData {
	TrackerDBManagerFlags flags;
};

struct TrackerTokenizer {
	TrackerTokenizerData *data;
	TrackerParser *parser;
};

struct TrackerTokenizerFunctionData {
	TrackerDBInterface *interface;
	TrackerDataManager *data_manager;
	gchar **property_names;
};

#define MAX_WORD_LENGTH 200
#define MAX_WORDS 10000

static int
tracker_tokenizer_create (void           *data,
                          const char    **argv,
                          int             argc,
                          Fts5Tokenizer **tokenizer_out)
{
	TrackerTokenizer *tokenizer;

	tokenizer = g_new0 (TrackerTokenizer, 1);
	tokenizer->data = data;
	tokenizer->parser = tracker_parser_new ();

	*tokenizer_out = (Fts5Tokenizer *) tokenizer;

	return SQLITE_OK;
}

static void
tracker_tokenizer_destroy (Fts5Tokenizer *fts5_tokenizer)
{
	TrackerTokenizer *tokenizer = (TrackerTokenizer *) fts5_tokenizer;

	tracker_parser_free (tokenizer->parser);
	g_free (tokenizer);
}

typedef int (*TokenFunc) (void       *pCtx,    /* Copy of 2nd argument to xTokenize() */
                          int         flags,   /* Mask of FTS5_TOKEN_* flags */
                          const char *token,   /* Pointer to buffer containing token */
                          int         n_token, /* Size of token in bytes */
                          int         start,   /* Byte offset of token within input text */
                          int         end);    /* Byte offset of end of token within input text */

static int
tracker_tokenizer_tokenize (Fts5Tokenizer *fts5_tokenizer,
                            void          *ctx,
                            int            flags, /* Mask of FTS5_TOKENIZE_* flags */
                            const char    *text,
                            int            length,
                            TokenFunc      token_func)
{
	TrackerTokenizer *tokenizer = (TrackerTokenizer *) fts5_tokenizer;
	TrackerTokenizerData *data = tokenizer->data;
	const gchar *token;
	int n_tokens = 0, pos, start, end, len;
	int rc = SQLITE_OK;

	if (length <= 0)
		return rc;

	tracker_parser_reset (tokenizer->parser, text, length,
			      MAX_WORD_LENGTH,
			      !!(data->flags & TRACKER_DB_MANAGER_FTS_ENABLE_STEMMER),
			      !!(data->flags & TRACKER_DB_MANAGER_FTS_ENABLE_UNACCENT),
			      !!(data->flags & TRACKER_DB_MANAGER_FTS_IGNORE_NUMBERS));

	while (n_tokens < MAX_WORDS) {
		token = tracker_parser_next (tokenizer->parser,
		                             &pos,
		                             &start, &end,
		                             &len);

		if (!token)
			break;

		rc = token_func (ctx, 0, token, len, start, end);

		if (rc != SQLITE_OK)
			break;

		n_tokens++;
	}

	return rc;
}

/* Our custom tokenizer: */
static const fts5_tokenizer tracker_tokenizer_module = {
	tracker_tokenizer_create,   /* xCreate   */
	tracker_tokenizer_destroy,  /* xDelete   */
	tracker_tokenizer_tokenize, /* xTokenize */
};

static TrackerTokenizerData *
tracker_tokenizer_data_new (TrackerDBManagerFlags flags)
{
	TrackerTokenizerData *p;

	p = g_new0 (TrackerTokenizerData, 1);
	p->flags = flags;

	return p;
}

static void
tracker_tokenizer_data_free (gpointer user_data)
{
	TrackerTokenizerData *data = user_data;

	g_free (data);
}

static int
offsets_tokenizer_func (void       *data,
                        int         flags,
                        const char *token,
                        int         n_token,
                        int         start,
                        int         end)
{
	GArray *offsets = data;
	g_array_append_val (offsets, start);
	return SQLITE_OK;
}

static gboolean
ensure_fts_properties (TrackerTokenizerFunctionData *data)
{
	TrackerOntologies *ontologies;
	TrackerProperty **properties;
	GArray *property_names;
	guint i, len;

	if (data->property_names)
		return data->property_names[0] != NULL;

	ontologies = tracker_data_manager_get_ontologies (data->data_manager);

	property_names = g_array_new (TRUE, FALSE, sizeof (gchar *));
	properties = tracker_ontologies_get_properties (ontologies, &len);

	for (i = 0; i < len; i++) {
		gchar *column;

		if (!tracker_property_get_fulltext_indexed (properties[i]))
			continue;

		column = g_strdup (tracker_property_get_name (properties[i]));
		g_array_append_val (property_names, column);
	}

	data->property_names = (gchar **) g_array_free (property_names, FALSE);

	return data->property_names[0] != NULL;
}

static void
tracker_offsets_function (const Fts5ExtensionApi  *api,
                          Fts5Context             *fts_ctx,
                          sqlite3_context         *ctx,
                          int                      n_args,
                          sqlite3_value          **args)
{
	TrackerTokenizerFunctionData *data;
	GString *str;
	int rc, n_hits, i;
	GArray *offsets = NULL;
	gint cur_col = -1;
	gboolean first = TRUE;

	if (n_args > 0) {
		sqlite3_result_error (ctx, "Invalid argument count", -1);
		return;
	}

	data = api->xUserData (fts_ctx);

	if (!ensure_fts_properties (data)) {
		sqlite3_result_null (ctx);
		return;
	}

	rc = api->xInstCount (fts_ctx, &n_hits);

	if (rc != SQLITE_OK) {
		sqlite3_result_null (ctx);
		return;
	}

	str = g_string_new (NULL);

	for (i = 0; i < n_hits; i++) {
		int phrase, col, n_token;

		rc = api->xInst (fts_ctx, i, &phrase, &col, &n_token);
		if (rc != SQLITE_OK)
			break;

		if (first || cur_col != col) {
			const char *text;
			int length;

			if (offsets)
				g_array_free (offsets, TRUE);

			offsets = g_array_new (FALSE, FALSE, sizeof (gint));
			rc = api->xColumnText (fts_ctx, col, &text, &length);
			if (rc != SQLITE_OK)
				break;

			rc = api->xTokenize (fts_ctx, text, length,
					     offsets, &offsets_tokenizer_func);
			if (rc != SQLITE_OK)
				break;

			cur_col = col;
		}

		first = FALSE;

		if (offsets->len != 0) {
			if (str->len != 0)
				g_string_append_c (str, ',');

			g_string_append_printf (str, "%s,%d",
						data->property_names[col],
						g_array_index (offsets, gint, n_token));
		}
	}

	if (offsets)
		g_array_free (offsets, TRUE);

	if (rc == SQLITE_OK) {
		gsize len = str->len;
		sqlite3_result_text (ctx,
		                     g_string_free (str, FALSE),
		                     len, g_free);
	} else {
		sqlite3_result_error_code (ctx, rc);
		g_string_free (str, TRUE);
	}
}

static fts5_api *
get_fts5_api (sqlite3  *db,
              GError  **error)
{
	int rc = SQLITE_OK;
	sqlite3_stmt *stmt;
	fts5_api *api = NULL;

	rc = sqlite3_prepare_v2(db, "SELECT fts5(?1)",
	                        -1, &stmt, 0);
	if (rc != SQLITE_OK)
		goto error;

	sqlite3_bind_pointer (stmt, 1, (void*) &api, "fts5_api_ptr", NULL);

	rc = sqlite3_step (stmt);
	if (rc != SQLITE_ROW)
		goto error;

	sqlite3_finalize (stmt);

	return api;

error:
	g_clear_pointer (&stmt, sqlite3_finalize);
	g_set_error (error,
	             TRACKER_DB_INTERFACE_ERROR,
	             TRACKER_DB_OPEN_ERROR,
	             "Could not override fts5 tokenizer: %s",
	             sqlite3_errstr (rc));
	return NULL;
}

static TrackerTokenizerFunctionData *
tracker_tokenizer_function_data_new (TrackerDBInterface *interface,
                                     TrackerDataManager *data_manager)
{
	TrackerTokenizerFunctionData *data;

	data = g_new0 (TrackerTokenizerFunctionData, 1);
	data->interface = interface;
	data->data_manager = data_manager;

	return data;
}

static void
tracker_tokenizer_function_data_free (TrackerTokenizerFunctionData *data)
{
	g_strfreev (data->property_names);
	g_free (data);
}

gboolean
tracker_tokenizer_initialize (sqlite3                *db,
                              TrackerDBInterface     *interface,
                              TrackerDBManagerFlags   flags,
                              TrackerDataManager     *data_manager,
                              GError                **error)
{
	TrackerTokenizerData *data;
	TrackerTokenizerFunctionData *func_data;
	fts5_tokenizer *tokenizer;
	fts5_api *api;

	api = get_fts5_api (db, error);

	if (!api)
		return FALSE;

	data = tracker_tokenizer_data_new (flags);
	tokenizer = (fts5_tokenizer *) &tracker_tokenizer_module;
	api->xCreateTokenizer (api, "TrackerTokenizer", data, tokenizer,
	                       tracker_tokenizer_data_free);

	/* Offsets */
	func_data = tracker_tokenizer_function_data_new (interface, data_manager);
	api->xCreateFunction (api, "tracker_offsets", func_data,
	                      &tracker_offsets_function,
	                      (GDestroyNotify) tracker_tokenizer_function_data_free);

	return TRUE;
}
