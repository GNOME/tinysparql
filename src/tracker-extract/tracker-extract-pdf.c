/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008-2011, Nokia <ivan.frade@nokia.com>
 * Copyright (C) 2010, Amit Aggarwal <amitcs06@gmail.com>
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

#include "config.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glib/poppler.h>

#include <gio/gunixoutputstream.h>
#include <gio/gunixinputstream.h>

#include <libtracker-common/tracker-date-time.h>
#include <libtracker-common/tracker-utils.h>
#include <libtracker-common/tracker-file-utils.h>

#include <libtracker-extract/tracker-extract.h>

#include "tracker-main.h"

/* Time in seconds before we kill the forked child process used for
 * content extraction */
#define EXTRACTION_PROCESS_TIMEOUT 10

/* Size of the buffer to use when reading, in bytes */
#define BUFFER_SIZE 65535

typedef struct {
	gchar *title;
	gchar *subject;
	gchar *creation_date;
	gchar *author;
	gchar *date;
	gchar *keywords;
} PDFData;

static void
read_toc (PopplerIndexIter  *index,
          GString          **toc)
{
	if (!index) {
		return;
	}

	if (!*toc) {
		*toc = g_string_new ("");
	}

	do {
		PopplerAction *action;
		PopplerIndexIter *iter;

		action = poppler_index_iter_get_action (index);

		if (!action) {
			continue;
		}

		switch (action->type) {
			case POPPLER_ACTION_GOTO_DEST: {
				PopplerActionGotoDest *ag = (PopplerActionGotoDest *)action;
				PopplerDest *agd = ag->dest;

				if (!tracker_is_empty_string (ag->title)) {
					g_string_append_printf (*toc, "%s ", ag->title);
				}

				if (!tracker_is_empty_string (agd->named_dest)) {
					g_string_append_printf (*toc, "%s ", agd->named_dest);
				}

				break;
			}

			case POPPLER_ACTION_LAUNCH: {
				PopplerActionLaunch *al = (PopplerActionLaunch *)action;

				if (!tracker_is_empty_string (al->title)) {
					g_string_append_printf (*toc, "%s ", al->title);
				}

				if (!tracker_is_empty_string (al->file_name)) {
					g_string_append_printf (*toc, "%s ", al->file_name);
				}

				if (!tracker_is_empty_string (al->params)) {
					g_string_append_printf (*toc, "%s ", al->params);
				}

				break;
			}

			case POPPLER_ACTION_URI: {
				PopplerActionUri *au = (PopplerActionUri *)action;

				if (!tracker_is_empty_string (au->uri)) {
					g_string_append_printf (*toc, "%s ", au->uri);
				}

				break;
			}

			case POPPLER_ACTION_NAMED: {
				PopplerActionNamed *an = (PopplerActionNamed *)action;

				if (!tracker_is_empty_string (an->title)) {
					g_string_append_printf (*toc, "%s, ", an->title);
				}

				if (!tracker_is_empty_string (an->named_dest)) {
					g_string_append_printf (*toc, "%s ", an->named_dest);
				}

				break;
			}

			case POPPLER_ACTION_MOVIE: {
				PopplerActionMovie *am = (PopplerActionMovie *)action;

				if (!tracker_is_empty_string (am->title)) {
					g_string_append_printf (*toc, "%s ", am->title);
				}

				break;
			}

			case POPPLER_ACTION_NONE:
			case POPPLER_ACTION_UNKNOWN:
			case POPPLER_ACTION_GOTO_REMOTE:
			case POPPLER_ACTION_RENDITION:
			case POPPLER_ACTION_OCG_STATE:
			case POPPLER_ACTION_JAVASCRIPT:
				/* Do nothing */
				break;
		}

		poppler_action_free (action);
		iter = poppler_index_iter_get_child (index);
		read_toc (iter, toc);
	} while (poppler_index_iter_next (index));

	poppler_index_iter_free (index);
}

static void
read_outline (PopplerDocument      *document,
              TrackerSparqlBuilder *metadata)
{
	PopplerIndexIter *index;
	GString *toc = NULL;

	index = poppler_index_iter_new (document);

	if (!index) {
		return;
	}

	read_toc (index, &toc);

	if (toc) {
		if (toc->len > 0) {
			tracker_sparql_builder_predicate (metadata, "nfo:tableOfContents");
			tracker_sparql_builder_object_unvalidated (metadata, toc->str);
		}

		g_string_free (toc, TRUE);
	}
}

static GString *
extract_content_text (PopplerDocument *document,
                      gsize            n_bytes)
{
	gint n_pages, i = 0;
	GString *string;
	GTimer *timer;
	gsize remaining_bytes = n_bytes;

	n_pages = poppler_document_get_n_pages (document);
	string = g_string_new ("");
	timer = g_timer_new ();

	while (i < n_pages &&
	       remaining_bytes > 0) {
		PopplerPage *page;
		gsize written_bytes = 0;
		gchar *text;

		page = poppler_document_get_page (document, i);
		i++;

		text = poppler_page_get_text (page);

		if (!text) {
			g_object_unref (page);
			continue;
		}

		if (tracker_text_validate_utf8 (text,
		                                MIN (strlen (text), remaining_bytes),
		                                &string,
		                                &written_bytes)) {
			g_string_append_c (string, ' ');
		}

		remaining_bytes -= written_bytes;

		g_debug ("Child: Extracted %" G_GSIZE_FORMAT " bytes from page %d, "
		         "%" G_GSIZE_FORMAT " bytes remaining",
		         written_bytes, i, remaining_bytes);

		g_free (text);
		g_object_unref (page);
	}

	g_debug ("Child: Content extraction finished: %d/%d pages indexed in %2.2f seconds, "
	         "%" G_GSIZE_FORMAT " bytes extracted",
	         i,
	         n_pages,
	         g_timer_elapsed (timer, NULL),
	         (n_bytes - remaining_bytes));

	g_timer_destroy (timer);

	return string;
}

static void
extract_content_child_process (PopplerDocument *document,
                               gsize            n_bytes,
                               int              fd[2])
{
	GString *str;
	gint64 size;
	GOutputStream *output_stream;
	GDataOutputStream *dataout_stream;

	/* This is the child extracting the content, hopefully in time */

	output_stream = g_unix_output_stream_new (fd[1], FALSE);
	dataout_stream = g_data_output_stream_new (output_stream);
	str = extract_content_text (document, n_bytes);
	size = (gint64) str->len;

	/* Write the results to the pipe */
	if (g_data_output_stream_put_int64 (dataout_stream, size, NULL, NULL)) {
		g_output_stream_write_all (output_stream,
		                           str->str,
		                           str->len,
		                           NULL,
		                           NULL,
		                           NULL);
	}

	g_debug ("Child: Content extraction now finished in child process, "
	         "written %" G_GSIZE_FORMAT " bytes to parent process",
	         size);

	g_string_free (str, TRUE);
	g_object_unref (dataout_stream);
	g_object_unref (output_stream);

	close (fd[1]);

	exit (0);
}

static gchar *
extract_content_parent_process (PopplerDocument *document,
                                int              fd[2],
                                pid_t            child_pid)
{
	GInputStream *input_stream;
	GDataInputStream *datain_stream;
	GString *content = NULL;
	GError *error = NULL;
	GTimer *timer = NULL;
	gsize bytes_expected = -1;
	gboolean timed_out = FALSE;
	gboolean finished = FALSE;
	struct timeval timeout;
	fd_set rfds;

	/* This is the parent process waiting for the content extractor to
	 * finish in time. */

	g_debug ("Parent: Content extraction now starting in child process (pid = %d)", child_pid);

	/* Set up gio streams */
	input_stream = g_unix_input_stream_new (fd[0], FALSE);
	datain_stream = g_data_input_stream_new (input_stream);

	/* Watch FD to see when it has input. */
	FD_ZERO(&rfds);
	FD_SET(fd[0], &rfds);

	/* We give the content extractor 10 seconds to do its job */
	timeout.tv_sec = EXTRACTION_PROCESS_TIMEOUT;
	timeout.tv_usec = 0;

	/* We also use our own timer because timeouts in select()
	 * can be inconsistent across UNIX platforms. Some update the
	 * timeout and some don't.
	 */
	timer = g_timer_new ();

	/* So, this is fairly simple, what we're doing here is using
	 * select() to know when the child process has written some or
	 * all the data and we then avoid the child blocking by
	 * reading from that stream. We couple with this with a
	 * timeout of 10 seconds so if we receive nothing then we know
	 * we can kill the process because it is taking too long.
	 *
	 * We use waitpid() to know if the process quit because it has
	 * finished or if it is still processing data and needs to be
	 * killed.
	 */
	while (!finished) {
		int retval;

		/* 1a. Wait for data on the FD and limit by timeout */
		retval = select (fd[0] + 1,  &rfds, NULL, NULL, &timeout);

		/* 2. Did we error? Have data? or just timeout? */
		if (retval == -1) {
			perror ("select()");
			finished = TRUE;
		} else if (retval == 1) {
			gsize bytes_remaining;
			gboolean read_finished = FALSE;

			if (g_timer_elapsed (timer, NULL) >= EXTRACTION_PROCESS_TIMEOUT) {
				finished = TRUE;
				timed_out = TRUE;
				continue;
			}

			/* 3. Start reading data */
			if (bytes_expected == -1) {
				/* We only need to read the size once before the data! */
				bytes_expected = (gsize) g_data_input_stream_read_int64 (datain_stream,
				                                                         NULL,
				                                                         &error);
				if (error) {
					g_warning ("Call to g_data_input_stream_read_int64() failed, %s",
					           error->message);
					g_error_free (error);
					finished = TRUE;
					continue;
				}

				g_debug ("Parent: Expected bytes to read is %" G_GSSIZE_FORMAT "", bytes_expected);
				bytes_remaining = bytes_expected;
				content = g_string_new ("");
			}

			/* 4. Read until done from stream and concatenate data */
			while (!read_finished) {
				gchar buf[BUFFER_SIZE];
				gsize bytes_read;

				memset (buf, 0, BUFFER_SIZE);
				bytes_read = g_input_stream_read (G_INPUT_STREAM (datain_stream),
				                                  buf,
				                                  MIN (BUFFER_SIZE, bytes_remaining),
				                                  NULL,
				                                  &error);

				g_debug ("Parent:   Bytes read is %" G_GSSIZE_FORMAT ","
				         "bytes remaining is %" G_GSSIZE_FORMAT "",
				         bytes_read,
				         MAX (bytes_remaining - bytes_read, 0));

				if (bytes_read == -1 || error) {
					g_warning ("Call to g_input_stream_read() failed, %s",
					           error ? error->message : "no error given");
					g_clear_error (&error);
					read_finished = TRUE;
					finished = TRUE;
				} else {
					content = g_string_append (content, buf);

					bytes_remaining -= bytes_read;
					bytes_remaining  = MAX (bytes_remaining, 0);

					if (bytes_read == 0) {
						/* We finished reading */
						g_debug ("Parent:   Finished reading all bytes");
						read_finished = TRUE;
					}

					/* Are we finished reading everything */
					if (bytes_remaining < 1) {
						finished = TRUE;
					}
				}
			}
		} else {
			/* 3. We must have timed out with no data in select() */
			finished = TRUE;
			timed_out = TRUE;
			g_debug ("Parent: Must have timed out with no data in select()");
		}
	}

	if (timed_out) {
		g_debug ("Parent: Child process took too long. We waited %d seconds, so we're going to kill it!",
		         EXTRACTION_PROCESS_TIMEOUT);
		kill (child_pid, SIGKILL);
	} else {
		g_debug ("Parent: Data received in %2.2f seconds (timeout is %d seconds)",
		         g_timer_elapsed (timer, NULL),
		         EXTRACTION_PROCESS_TIMEOUT);
	}

	g_timer_destroy (timer);

	g_object_unref (datain_stream);
	g_object_unref (input_stream);

	close (fd[0]);

	return content ? g_string_free (content, FALSE) : NULL;
}

static void
extract_content_child_cleanup (int action)
{
	pid_t child_pid;
	int status;

	g_debug ("Parent: Zombies, say hello to my little friend!");
	while ((child_pid = waitpid (-1, &status, WNOHANG)) > 0) {
		g_debug ("Parent:   Zombie %d reaped", child_pid);
	}
}

static gchar *
extract_content (PopplerDocument *document,
                 gsize            n_bytes)
{
	pid_t child_pid;
	int fd[2];
	sigset_t mask;
	sigset_t orig_mask;
	struct sigaction sa;

	if (pipe (fd) == -1) {
		g_warning ("Content extraction failed, call to pipe() failed");
		return NULL;
	}

	/* Set sig mask before fork() to avoid race conditions */
	sigemptyset (&mask);
	sigaddset (&mask, SIGCHLD);

	/* Add zombie handler */
	sigfillset (&sa.sa_mask);
	sa.sa_handler = extract_content_child_cleanup;
	sa.sa_flags = 0;
	sigaction (SIGCHLD, &sa, NULL);

	if (sigprocmask (SIG_SETMASK, &mask, &orig_mask) == -1) {
		g_warning ("Content extraction failed, call to sigprocmask() failed");
		return NULL;
	}

	child_pid = fork ();

	if (child_pid == -1) {
		g_warning ("Content extraction failed, call to fork() failed");

		close (fd[0]);
		close (fd[1]);
	}

	if (child_pid == 0) {
		extract_content_child_process (document, n_bytes, fd);
		return NULL;
	}

	return extract_content_parent_process (document, fd, child_pid);
}

static void
write_pdf_data (PDFData               data,
                TrackerSparqlBuilder *metadata,
                GPtrArray            *keywords)
{
	if (!tracker_is_empty_string (data.title)) {
		tracker_sparql_builder_predicate (metadata, "nie:title");
		tracker_sparql_builder_object_unvalidated (metadata, data.title);
	}

	if (!tracker_is_empty_string (data.subject)) {
		tracker_sparql_builder_predicate (metadata, "nie:subject");
		tracker_sparql_builder_object_unvalidated (metadata, data.subject);
	}

	if (!tracker_is_empty_string (data.author)) {
		tracker_sparql_builder_predicate (metadata, "nco:creator");
		tracker_sparql_builder_object_blank_open (metadata);
		tracker_sparql_builder_predicate (metadata, "a");
		tracker_sparql_builder_object (metadata, "nco:Contact");
		tracker_sparql_builder_predicate (metadata, "nco:fullname");
		tracker_sparql_builder_object_unvalidated (metadata, data.author);
		tracker_sparql_builder_object_blank_close (metadata);
	}

	if (!tracker_is_empty_string (data.date)) {
		tracker_sparql_builder_predicate (metadata, "nie:contentCreated");
		tracker_sparql_builder_object_unvalidated (metadata, data.date);
	}

	if (!tracker_is_empty_string (data.keywords)) {
		tracker_keywords_parse (keywords, data.keywords);
	}
}

G_MODULE_EXPORT gboolean
tracker_extract_get_metadata (TrackerExtractInfo *info)
{
	TrackerConfig *config;
	GTime creation_date;
	GError *error = NULL;
	TrackerSparqlBuilder *metadata, *preupdate;
	const gchar *graph;
	TrackerXmpData *xd = NULL;
	PDFData pd = { 0 }; /* actual data */
	PDFData md = { 0 }; /* for merging */
	PopplerDocument *document;
	gchar *xml = NULL;
	gchar *content, *uri;
	guint n_bytes;
	GPtrArray *keywords;
	GString *where;
	guint i;
	GFile *file;
	gchar *filename;
	int fd;
	gchar *contents = NULL;
	gsize len;
	struct stat st;

	metadata = tracker_extract_info_get_metadata_builder (info);
	preupdate = tracker_extract_info_get_preupdate_builder (info);
	graph = tracker_extract_info_get_graph (info);

	file = tracker_extract_info_get_file (info);
	filename = g_file_get_path (file);

	fd = tracker_file_open_fd (filename);

	if (fd == -1) {
		g_warning ("Could not open pdf file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		g_free (filename);
		return FALSE;
	}

	if (fstat (fd, &st) == -1) {
		g_warning ("Could not fstat pdf file '%s': %s\n",
		           filename,
		           g_strerror (errno));
		close (fd);
		g_free (filename);
		return FALSE;
	}

	if (st.st_size == 0) {
		contents = NULL;
		len = 0;
	} else {
		contents = (gchar *) mmap (NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
		if (contents == NULL || contents == MAP_FAILED) {
			g_warning ("Could not mmap pdf file '%s': %s\n",
			           filename,
			           g_strerror (errno));
			close (fd);
			g_free (filename);
			return FALSE;
		}
		len = st.st_size;
	}

	g_free (filename);
	uri = g_file_get_uri (file);

	document = poppler_document_new_from_data (contents, len, NULL, &error);
	
	if (error) {
		if (error->code == POPPLER_ERROR_ENCRYPTED) {
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

			tracker_sparql_builder_predicate (metadata, "nfo:isContentEncrypted");
			tracker_sparql_builder_object_boolean (metadata, TRUE);

			g_error_free (error);
			g_free (uri);
			close (fd);

			return TRUE;
		} else {
			g_warning ("Couldn't create PopplerDocument from uri:'%s', %s",
			           uri,
			           error->message ? error->message : "no error given");

			g_error_free (error);
			g_free (uri);
			close (fd);

			return FALSE;
		}
	}

	if (!document) {
		g_warning ("Could not create PopplerDocument from uri:'%s', "
		           "NULL returned without an error",
		           uri);
		g_free (uri);
		close (fd);
		return FALSE;
	}

	tracker_sparql_builder_predicate (metadata, "a");
	tracker_sparql_builder_object (metadata, "nfo:PaginatedTextDocument");

	g_object_get (document,
	              "title", &pd.title,
	              "author", &pd.author,
	              "subject", &pd.subject,
	              "keywords", &pd.keywords,
	              "creation-date", &creation_date,
	              "metadata", &xml,
	              NULL);

	if (creation_date > 0) {
		pd.creation_date = tracker_date_to_string ((time_t) creation_date);
	}

	keywords = g_ptr_array_new ();

	if (xml &&
	    (xd = tracker_xmp_new (xml, strlen (xml), uri)) != NULL) {
		/* The casts here are well understood and known */
		md.title = (gchar *) tracker_coalesce_strip (4, pd.title, xd->title, xd->title2, xd->pdf_title);
		md.subject = (gchar *) tracker_coalesce_strip (2, pd.subject, xd->subject);
		md.date = (gchar *) tracker_coalesce_strip (3, pd.creation_date, xd->date, xd->time_original);
		md.author = (gchar *) tracker_coalesce_strip (2, pd.author, xd->creator);

		write_pdf_data (md, metadata, keywords);

		if (xd->keywords) {
			tracker_keywords_parse (keywords, xd->keywords);
		}

		if (xd->pdf_keywords) {
			tracker_keywords_parse (keywords, xd->pdf_keywords);
		}

		if (xd->publisher) {
			tracker_sparql_builder_predicate (metadata, "nco:publisher");
			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");
			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, xd->publisher);
			tracker_sparql_builder_object_blank_close (metadata);
		}

		if (xd->type) {
			tracker_sparql_builder_predicate (metadata, "dc:type");
			tracker_sparql_builder_object_unvalidated (metadata, xd->type);
		}

		if (xd->format) {
			tracker_sparql_builder_predicate (metadata, "dc:format");
			tracker_sparql_builder_object_unvalidated (metadata, xd->format);
		}

		if (xd->identifier) {
			tracker_sparql_builder_predicate (metadata, "dc:identifier");
			tracker_sparql_builder_object_unvalidated (metadata, xd->identifier);
		}

		if (xd->source) {
			tracker_sparql_builder_predicate (metadata, "dc:source");
			tracker_sparql_builder_object_unvalidated (metadata, xd->source);
		}

		if (xd->language) {
			tracker_sparql_builder_predicate (metadata, "dc:language");
			tracker_sparql_builder_object_unvalidated (metadata, xd->language);
		}

		if (xd->relation) {
			tracker_sparql_builder_predicate (metadata, "dc:relation");
			tracker_sparql_builder_object_unvalidated (metadata, xd->relation);
		}

		if (xd->coverage) {
			tracker_sparql_builder_predicate (metadata, "dc:coverage");
			tracker_sparql_builder_object_unvalidated (metadata, xd->coverage);
		}

		if (xd->license) {
			tracker_sparql_builder_predicate (metadata, "nie:license");
			tracker_sparql_builder_object_unvalidated (metadata, xd->license);
		}

		if (xd->make || xd->model) {
			gchar *equip_uri;

			equip_uri = tracker_sparql_escape_uri_printf ("urn:equipment:%s:%s:",
			                                              xd->make ? xd->make : "",
			                                              xd->model ? xd->model : "");

			tracker_sparql_builder_insert_open (preupdate, NULL);
			if (graph) {
				tracker_sparql_builder_graph_open (preupdate, graph);
			}

			tracker_sparql_builder_subject_iri (preupdate, equip_uri);
			tracker_sparql_builder_predicate (preupdate, "a");
			tracker_sparql_builder_object (preupdate, "nfo:Equipment");

			if (xd->make) {
				tracker_sparql_builder_predicate (preupdate, "nfo:manufacturer");
				tracker_sparql_builder_object_unvalidated (preupdate, xd->make);
			}

			if (xd->model) {
				tracker_sparql_builder_predicate (preupdate, "nfo:model");
				tracker_sparql_builder_object_unvalidated (preupdate, xd->model);
			}

			if (graph) {
				tracker_sparql_builder_graph_close (preupdate);
			}
			tracker_sparql_builder_insert_close (preupdate);

			tracker_sparql_builder_predicate (metadata, "nfo:equipment");
			tracker_sparql_builder_object_iri (metadata, equip_uri);
			g_free (equip_uri);
		}

		if (xd->orientation) {
			tracker_sparql_builder_predicate (metadata, "nfo:orientation");
			tracker_sparql_builder_object (metadata, xd->orientation);
		}

		if (xd->rights) {
			tracker_sparql_builder_predicate (metadata, "nie:copyright");
			tracker_sparql_builder_object_unvalidated (metadata, xd->rights);
		}

		if (xd->white_balance) {
			tracker_sparql_builder_predicate (metadata, "nmm:whiteBalance");
			tracker_sparql_builder_object (metadata, xd->white_balance);
		}

		if (xd->fnumber) {
			gdouble value;

			value = g_strtod (xd->fnumber, NULL);
			tracker_sparql_builder_predicate (metadata, "nmm:fnumber");
			tracker_sparql_builder_object_double (metadata, value);
		}

		if (xd->flash) {
			tracker_sparql_builder_predicate (metadata, "nmm:flash");
			tracker_sparql_builder_object (metadata, xd->flash);
		}

		if (xd->focal_length) {
			gdouble value;

			value = g_strtod (xd->focal_length, NULL);
			tracker_sparql_builder_predicate (metadata, "nmm:focalLength");
			tracker_sparql_builder_object_double (metadata, value);
		}

		/* Question: Shouldn't xd->Artist be merged with md.author instead? */

		if (xd->artist || xd->contributor) {
			const gchar *artist;

			artist = tracker_coalesce_strip (2, xd->artist, xd->contributor);
			tracker_sparql_builder_predicate (metadata, "nco:contributor");
			tracker_sparql_builder_object_blank_open (metadata);
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "nco:Contact");
			tracker_sparql_builder_predicate (metadata, "nco:fullname");
			tracker_sparql_builder_object_unvalidated (metadata, artist);
			tracker_sparql_builder_object_blank_close (metadata);
		}

		if (xd->exposure_time) {
			gdouble value;

			value = g_strtod (xd->exposure_time, NULL);
			tracker_sparql_builder_predicate (metadata, "nmm:exposureTime");
			tracker_sparql_builder_object_double (metadata, value);
		}

		if (xd->iso_speed_ratings) {
			gdouble value;

			value = g_strtod (xd->iso_speed_ratings, NULL);
			tracker_sparql_builder_predicate (metadata, "nmm:isoSpeed");
			tracker_sparql_builder_object_double (metadata, value);
		}

		if (xd->description) {
			tracker_sparql_builder_predicate (metadata, "nie:description");
			tracker_sparql_builder_object_unvalidated (metadata, xd->description);
		}

		if (xd->metering_mode) {
			tracker_sparql_builder_predicate (metadata, "nmm:meteringMode");
			tracker_sparql_builder_object (metadata, xd->metering_mode);
		}

		if (xd->address || xd->state || xd->country || xd->city ||
		    xd->gps_altitude || xd->gps_latitude || xd-> gps_longitude) {

			tracker_sparql_builder_predicate (metadata, "slo:location");

			tracker_sparql_builder_object_blank_open (metadata); /* GeoLocation */
			tracker_sparql_builder_predicate (metadata, "a");
			tracker_sparql_builder_object (metadata, "slo:GeoLocation");
			
			if (xd->address || xd->state || xd->country || xd->city)  {
				gchar *addruri;
				addruri = tracker_sparql_get_uuid_urn ();

				tracker_sparql_builder_predicate (metadata, "slo:postalAddress");
				tracker_sparql_builder_object_iri (metadata, addruri);			

				tracker_sparql_builder_insert_open (preupdate, NULL);
				if (graph) {
					tracker_sparql_builder_graph_open (preupdate, graph);
				}

				tracker_sparql_builder_subject_iri (preupdate, addruri);

				g_free (addruri);

				tracker_sparql_builder_predicate (preupdate, "a");
				tracker_sparql_builder_object (preupdate, "nco:PostalAddress");

				if (xd->address) {
				        tracker_sparql_builder_predicate (preupdate, "nco:streetAddress");
				        tracker_sparql_builder_object_unvalidated (preupdate, xd->address);
				}

				if (xd->state) {
				        tracker_sparql_builder_predicate (preupdate, "nco:region");
				        tracker_sparql_builder_object_unvalidated (preupdate, xd->state);
				}

				if (xd->city) {
				        tracker_sparql_builder_predicate (preupdate, "nco:locality");
				        tracker_sparql_builder_object_unvalidated (preupdate, xd->city);
				}

				if (xd->country) {
				        tracker_sparql_builder_predicate (preupdate, "nco:country");
				        tracker_sparql_builder_object_unvalidated (preupdate, xd->country);
				}

				if (graph) {
					tracker_sparql_builder_graph_close (preupdate);
				}
				tracker_sparql_builder_insert_close (preupdate);
			}

			if (xd->gps_altitude) {
				tracker_sparql_builder_predicate (metadata, "slo:altitude");
				tracker_sparql_builder_object_unvalidated (metadata, xd->gps_altitude);
			}

			if (xd->gps_latitude) {
				tracker_sparql_builder_predicate (metadata, "slo:latitude");
				tracker_sparql_builder_object_unvalidated (metadata, xd->gps_latitude);
			}

			if (xd->gps_longitude) {
				tracker_sparql_builder_predicate (metadata, "slo:longitude");
				tracker_sparql_builder_object_unvalidated (metadata, xd->gps_longitude);
			}

			tracker_sparql_builder_object_blank_close (metadata); /* GeoLocation */
		}

                if (xd->regions) {
	                tracker_xmp_apply_regions (preupdate, metadata, graph, xd);
                }

		tracker_xmp_free (xd);
	} else {
		/* So if we are here we have NO XMP data and we just
		 * write what we know from Poppler.
		 */
		write_pdf_data (pd, metadata, keywords);
	}

	where = g_string_new ("");

	for (i = 0; i < keywords->len; i++) {
		gchar *p, *escaped, *var;

		p = g_ptr_array_index (keywords, i);
		escaped = tracker_sparql_escape_string (p);
		var = g_strdup_printf ("tag%d", i + 1);

		/* ensure tag with specified label exists */
		tracker_sparql_builder_append (preupdate, "INSERT { ");

		if (graph) {
			tracker_sparql_builder_append (preupdate, "GRAPH <");
			tracker_sparql_builder_append (preupdate, graph);
			tracker_sparql_builder_append (preupdate, "> { ");
		}

		tracker_sparql_builder_append (preupdate,
		                               "_:tag a nao:Tag ; nao:prefLabel \"");
		tracker_sparql_builder_append (preupdate, escaped);
		tracker_sparql_builder_append (preupdate, "\"");

		if (graph) {
			tracker_sparql_builder_append (preupdate, " } ");
		}

		tracker_sparql_builder_append (preupdate, " }\n");
		tracker_sparql_builder_append (preupdate,
		                               "WHERE { FILTER (NOT EXISTS { "
		                               "?tag a nao:Tag ; nao:prefLabel \"");
		tracker_sparql_builder_append (preupdate, escaped);
		tracker_sparql_builder_append (preupdate,
		                               "\" }) }\n");

		/* associate file with tag */
		tracker_sparql_builder_predicate (metadata, "nao:hasTag");
		tracker_sparql_builder_object_variable (metadata, var);

		g_string_append_printf (where, "?%s a nao:Tag ; nao:prefLabel \"%s\" .\n", var, escaped);

		g_free (var);
		g_free (escaped);
		g_free (p);
	}
	g_ptr_array_free (keywords, TRUE);

	tracker_extract_info_set_where_clause (info, where->str);
	g_string_free (where, TRUE);

	tracker_sparql_builder_predicate (metadata, "nfo:pageCount");
	tracker_sparql_builder_object_int64 (metadata, poppler_document_get_n_pages (document));

	config = tracker_main_get_config ();
	n_bytes = tracker_config_get_max_bytes (config);
	content = extract_content (document, n_bytes);

	if (content) {
		tracker_sparql_builder_predicate (metadata, "nie:plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, content);
		g_free (content);
	}

	read_outline (document, metadata);

	g_free (xml);
	g_free (pd.keywords);
	g_free (pd.title);
	g_free (pd.subject);
	g_free (pd.creation_date);
	g_free (pd.author);
	g_free (pd.date);
	g_free (uri);

	g_object_unref (document);

	if (contents) {
		munmap (contents, len);
	}

	close (fd);

	return TRUE;
}
