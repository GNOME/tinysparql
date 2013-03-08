/*
 * Copyright (C) 2012, Red Hat, Inc.
 *
 *   Code adapted from evince/backend/dvi/mdvi-lib/dviread.c
 *   Copyright (C) 2000, Matias Atria
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <glib.h>
#include <gmodule.h>

#include <libtracker-extract/tracker-extract.h>

#define __PROTO(x)	x
extern gulong	fugetn __PROTO((FILE *, size_t));

#define fgetbyte(p)	((unsigned)getc(p))
#define fuget4(p)	fugetn((p), 4)
#define fuget3(p)	fugetn((p), 3)
#define fuget2(p)	fugetn((p), 2)
#define fuget1(p)	fgetbyte(p)


#define DVI_ID		2
#define DVI_TRAILER	223
#define DVI_PRE		247
#define DVI_POST	248

typedef struct {
	char *filename;	/* name of the DVI file */
	FILE *in;	/* from here we read */
	char *fileid;	/* from preamble */
	int npages;	/* number of pages */
	int depth;	/* recursion depth */
	gint32 num;	/* numerator */
	gint32 den;	/* denominator */
	gint32 dvimag;	/* original magnification */
	int dvi_page_w;	/* unscaled page width */
	int dvi_page_h;	/* unscaled page height */
	int stacksize;	/* stack depth */
} DviContext;

gulong
fugetn (FILE *p, size_t n)
{
	gulong v;

	v = fgetbyte(p);
	while (--n > 0) {
		v = (v << 8) | fgetbyte(p);
	}

	return v;
}

static char *
opendvi (const char *name)
{
	int len;

	len = strlen (name);

	/* if file ends with .dvi and it exists, that's it */
	if (len >= 4 && g_strcmp0 (name + len - 4, ".dvi") == 0) {
		g_debug ("Opening filename:'%s'", name);

		if (access (name, R_OK) == 0) {
			return g_strdup (name);
		}
	}

	return NULL;
}

static void
mdvi_destroy_context (DviContext *dvi)
{
	g_free (dvi->filename);
	g_free (dvi->fileid);

	if (dvi->in) {
		fclose (dvi->in);
	}

	g_free (dvi);
}

static DviContext *
mdvi_init_context (const char *file)
{
	FILE *p;
	gint32 arg;
	int op;
	int n;
	DviContext *dvi;
	char *filename;

	/*
	 * 1. Open the file and initialize the DVI context
	 */
	filename = opendvi (file);
	if (filename == NULL) {
		return NULL;
	}

	p = fopen (filename, "rb");
	if (p == NULL) {
		g_free (filename);
		return NULL;
	}

	dvi = g_new0 (DviContext, 1);
	dvi->filename = filename;
	dvi->in = p;

	/*
	 * 2. Read the preamble, extract scaling information
	 */
	if (fuget1 (p) != DVI_PRE) {
		goto error;
	}

	if ((arg = fuget1 (p)) != DVI_ID) {
		g_message ("Unsupported DVI format (version %u)", arg);
		goto error;
	}

	/* get dimensions */
	dvi->num = fuget4 (p);
	dvi->den = fuget4 (p);
	dvi->dvimag = fuget4 (p);

	/* check that these numbers make sense */
	if (!dvi->num || !dvi->den || !dvi->dvimag) {
		goto error;
	}

	/* get the comment from the preamble */
	n = fuget1 (p);
	dvi->fileid = g_malloc (n + 1);
	fread (dvi->fileid, 1, n, p);
	dvi->fileid[n] = 0;
	g_debug ("Preamble Comment: '%s'", dvi->fileid);

	/*
	 * 3. Read postamble, extract page information (number of
	 *    pages, dimensions) and stack depth.
	 */

	/* jump to the end of the file */
	if (fseek (p, (long) - 1, SEEK_END) == -1) {
		goto error;
	}

	for (n = 0; (op = fuget1 (p)) == DVI_TRAILER; n++) {
		if (fseek (p, (long) - 2, SEEK_CUR) < 0) {
			break;
		}
	}

	if (op != arg || n < 4) {
		goto error;
	}

	/* get the pointer to postamble */
	fseek (p, (long) - 5, SEEK_CUR);
	arg = fuget4 (p);

	/* jump to it */
	fseek (p, (long) arg, SEEK_SET);
	if (fuget1 (p) != DVI_POST) {
		goto error;
	}

	fuget4 (p); /* offset */
	if (dvi->num != fuget4 (p) ||
	    dvi->den != fuget4 (p) ||
	    dvi->dvimag != fuget4 (p)) {
		goto error;
	}
	dvi->dvi_page_h = fuget4 (p);
	dvi->dvi_page_w = fuget4 (p);
	dvi->stacksize = fuget2 (p);
	dvi->npages = fuget2 (p);

	g_debug ("Postamble: %d pages", dvi->npages);

	return dvi;

error:
	mdvi_destroy_context (dvi);
	return NULL;
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerSparqlBuilder *metadata;
	GFile *file;
	gchar *filename;
	DviContext *context;

	metadata = tracker_extract_info_get_metadata_builder (info);
	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);

	context = mdvi_init_context (filename);

	if (context == NULL) {
		g_warning ("Could not open dvi file '%s'\n", filename);
		g_free (filename);
		return FALSE;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
	tracker_sparql_builder_object_int64 (metadata, context->npages);

	if (context->fileid) {
		tracker_sparql_builder_predicate (metadata, "nie:comment");
		tracker_sparql_builder_object_unvalidated (metadata, context->fileid);
	}

	mdvi_destroy_context (context);

	return TRUE;
}
