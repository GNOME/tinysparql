/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
 * Copyright (C) 2008, Nokia

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include <gio/gio.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus-glib-lowlevel.h>

#include <libtracker-common/tracker-file-utils.h>
#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-ontology.h>
#include <libtracker-common/tracker-thumbnailer.h>

#include "tracker-module-metadata-utils.h"
#include "tracker-extract-client.h"
#include "tracker-dbus.h"

#define METADATA_FILE_PATH           "File:Path"
#define METADATA_FILE_NAME           "File:Name"
#define METADATA_FILE_LINK           "File:Link"
#define METADATA_FILE_MIMETYPE       "File:Mime"
#define METADATA_FILE_SIZE           "File:Size"
#define METADATA_FILE_MODIFIED       "File:Modified"
#define METADATA_FILE_ACCESSED       "File:Accessed"
#define METADATA_FILE_ADDED          "File:Added"

#undef  TRY_LOCALE_TO_UTF8_CONVERSION
#define TEXT_MAX_SIZE                1048576  /* bytes */
#define TEXT_CHECK_SIZE              65535    /* bytes */

#define TEXT_EXTRACTION_TIMEOUT      10

typedef struct {
        GPid pid;
        guint stdout_watch_id;
        GIOChannel *stdin_channel;
        GIOChannel *stdout_channel;
        GMainLoop  *data_incoming_loop;
        gpointer data;
} ProcessContext;

typedef struct {
        GHashTable *metadata;
        GMainLoop *main_loop;
        GPid pid;
} ExtractorContext;

static GPid extractor_pid = 0;


static DBusGProxy * get_dbus_extract_proxy (void);

static GPid
get_extractor_pid (void)
{
	GError *error = NULL;
	GPid pid;

	/* Get new PID from extractor */
	if (!org_freedesktop_Tracker_Extract_get_pid (get_dbus_extract_proxy (),
						      &pid,
						      &error)) {
		g_critical ("Couldn't get PID from tracker-extract, %s",
			    error ? error->message : "no error given");
		g_clear_error (&error);
		pid = 0;
	}

	g_message ("New extractor PID is %d", (guint) pid);

	return pid;
}

static void
extractor_changed_availability_cb (const gchar *name,
				   gboolean     available,
				   gpointer     user_data)
{
	if (!available) {
		/* invalidate PID */
		extractor_pid = 0;
	} else {
		extractor_pid = get_extractor_pid ();
	}
}

static DBusGProxy *
get_dbus_extract_proxy (void)
{
        static DBusGProxy *proxy = NULL;
        DBusGConnection *connection;
        GError *error = NULL;

        /* FIXME: Not perfect, we leak */
        if (G_LIKELY (proxy)) {
                return proxy;
        }

        connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

        if (!connection) {
                g_critical ("Could not connect to the DBus session bus, %s",
                            error ? error->message : "no error given.");
                g_clear_error (&error);
                return FALSE;
        }

        /* Get proxy for Service / Path / Interface of the indexer */
        proxy = dbus_g_proxy_new_for_name (connection,
                                           "org.freedesktop.Tracker.Extract",
                                           "/org/freedesktop/Tracker/Extract",
                                           "org.freedesktop.Tracker.Extract");

        if (!proxy) {
                g_critical ("Could not create a DBusGProxy to the extract service");
        }

	tracker_dbus_add_name_monitor ("org.freedesktop.Tracker.Extract",
				       extractor_changed_availability_cb,
				       NULL, NULL);
        return proxy;
}

static void
process_context_destroy (ProcessContext *context)
{
        if (context->stdin_channel) {
                g_io_channel_shutdown (context->stdin_channel, FALSE, NULL);
                g_io_channel_unref (context->stdin_channel);
                context->stdin_channel = NULL;
        }

        if (context->stdout_watch_id != 0) {
                g_source_remove (context->stdout_watch_id);
                context->stdout_watch_id = 0;
        }

        if (context->stdout_channel) {
                g_io_channel_shutdown (context->stdout_channel, FALSE, NULL);
                g_io_channel_unref (context->stdout_channel);
                context->stdout_channel = NULL;
        }

        if (context->data_incoming_loop) {
                if (g_main_loop_is_running (context->data_incoming_loop)) {
                        g_main_loop_quit (context->data_incoming_loop);
                }

                g_main_loop_unref (context->data_incoming_loop);
                context->data_incoming_loop = NULL;
        }

        if (context->pid != 0) {
                g_spawn_close_pid (context->pid);
                context->pid = 0;
        }

        g_free (context);
}

static void
process_context_kill (ProcessContext *context)
{
        g_message ("Attempting to kill text filter with SIGKILL");

        if (kill (context->pid, SIGKILL) == -1) {
                const gchar *str = g_strerror (errno);

                g_message ("  Could not kill process %d, %s",
                           context->pid,
                           str ? str : "no error given");
        } else {
                g_message ("  Killed process %d", context->pid);
        }
}

static void
process_context_child_watch_cb (GPid     pid,
                                gint     status,
                                gpointer user_data)
{
        ProcessContext *context;

        g_debug ("Process '%d' exited with code %d",
                 pid,
                 status);

        context = (ProcessContext *) user_data;
        process_context_destroy (context);
}

static ProcessContext *
process_context_create (const gchar **argv,
                        GIOFunc       stdout_watch_func)
{
        ProcessContext *context;
        GIOChannel *stdin_channel, *stdout_channel;
        GIOFlags flags;
        GPid pid;

        if (!tracker_spawn_async_with_channels (argv,
                                                TEXT_EXTRACTION_TIMEOUT,
                                                &pid,
                                                &stdin_channel,
                                                &stdout_channel,
                                                NULL)) {
                return NULL;
        }

        g_debug ("Process '%d' spawned for command:'%s'",
                 pid,
                 argv[0]);

        context = g_new0 (ProcessContext, 1);
        context->pid = pid;
        context->stdin_channel = stdin_channel;
        context->stdout_channel = stdout_channel;
        context->data_incoming_loop = g_main_loop_new (NULL, FALSE);
        context->stdout_watch_id = g_io_add_watch (stdout_channel,
                                                   G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
                                                   stdout_watch_func,
                                                   context);

        flags = g_io_channel_get_flags (context->stdout_channel);
        flags |= G_IO_FLAG_NONBLOCK;

        g_io_channel_set_flags (context->stdout_channel, flags, NULL);

        g_child_watch_add (context->pid, process_context_child_watch_cb, context);

        return context;
}

static ExtractorContext *
extractor_context_create (TrackerModuleMetadata *metadata)
{
        ExtractorContext *context;

	if (G_UNLIKELY (extractor_pid == 0)) {
		/* Ensure we have a PID to kill if anything goes wrong */
		extractor_pid = get_extractor_pid ();
	}

        context = g_slice_new0 (ExtractorContext);
        context->main_loop = g_main_loop_new (NULL, FALSE);
        context->metadata = g_object_ref (metadata);
        context->pid = extractor_pid;

        return context;
}

static void
extractor_context_destroy (ExtractorContext *context)
{
        g_object_unref (context->metadata);
        g_main_loop_unref (context->main_loop);
        g_slice_free (ExtractorContext, context);
}

static void
extractor_context_kill (ExtractorContext *context)
{
	g_message ("Attempting to kill tracker-extract with SIGKILL");

	if (context->pid == 0) {
		g_warning ("  No PID for tracker-extract");
		return;
	}

        if (kill (context->pid, SIGKILL) == -1) {
                const gchar *str = g_strerror (errno);

                g_message ("  Could not kill process %d, %s",
                           context->pid,
                           str ? str : "no error given");
        } else {
                g_message ("  Killed process %d", context->pid);
        }
}

static void
metadata_utils_add_embedded_data (TrackerModuleMetadata *metadata,
                                  TrackerField          *field,
                                  const gchar           *value)
{
        gchar *utf_value;

        if (!g_utf8_validate (value, -1, NULL)) {
                utf_value = g_locale_to_utf8 (value, -1, NULL, NULL, NULL);
        } else {
                utf_value = g_strdup (value);
        }

        if (utf_value) {
                const gchar *name;

                name = tracker_field_get_name (field);

                if (tracker_field_get_data_type (field) == TRACKER_FIELD_TYPE_DATE) {
                        gchar *time_str;

                        /* Dates come in ISO 8601 format, we handle them as time_t */
                        time_str = tracker_date_to_time_string (utf_value);

			if (time_str) {
				tracker_module_metadata_add_string (metadata, name, time_str);
				g_free (time_str);
			}
                } else {
                        tracker_module_metadata_add_string (metadata, name, utf_value);
                }

                g_free (utf_value);
        }
}

static void
metadata_utils_get_embedded_foreach (gpointer key,
                                     gpointer value,
                                     gpointer user_data)
{
        TrackerModuleMetadata *metadata;
        TrackerField *field;
        gchar *key_str;
        gchar *value_str;

        metadata = user_data;
        key_str = key;
        value_str = value;
        
        if (!key || !value) {
                return;
        }
        
        field = tracker_ontology_get_field_by_name (key_str);
        if (!field) {
                g_warning ("Field name '%s' isn't described in the ontology", key_str);
                return;
        }
        
        if (tracker_field_get_multiple_values (field)) {
                GStrv strv;
                guint i;
                
                strv = g_strsplit (value_str, "|", -1);
                
                for (i = 0; strv[i]; i++) {
                        metadata_utils_add_embedded_data (metadata, field, strv[i]);
                }
                
                g_strfreev (strv);
        } else {
                metadata_utils_add_embedded_data (metadata, field, value_str);
        }
}

static void
get_metadata_async_cb (DBusGProxy *proxy,
                       GHashTable *values,
                       GError     *error,
                       gpointer    user_data)
{
        ExtractorContext *context;
        gboolean should_kill = TRUE;

        context = (ExtractorContext *) user_data;

        if (error) {
                switch (error->code) {
                case DBUS_GERROR_FAILED:
                case DBUS_GERROR_NO_MEMORY:
                case DBUS_GERROR_NO_REPLY:
                case DBUS_GERROR_IO_ERROR:
                case DBUS_GERROR_LIMITS_EXCEEDED:
                case DBUS_GERROR_TIMEOUT:
                case DBUS_GERROR_DISCONNECTED:
                case DBUS_GERROR_TIMED_OUT:
                case DBUS_GERROR_REMOTE_EXCEPTION:
                        break;

                default:
                        should_kill = FALSE;
                        break;
                }

                g_message ("Couldn't extract metadata, %s",
                           error->message);

                g_clear_error (&error);

                if (should_kill) {
                        extractor_context_kill (context);
                }
        } else if (values) {
                g_hash_table_foreach (values,
                                      metadata_utils_get_embedded_foreach,
                                      context->metadata);

                g_hash_table_destroy (values);
        }

        g_main_loop_quit (context->main_loop);
}

static void
metadata_utils_get_embedded (GFile                 *file,
                             const char            *mime_type,
                             TrackerModuleMetadata *metadata)
{
        ExtractorContext *context;
        const gchar *service_type;
        gchar *path;

        service_type = tracker_ontology_get_service_by_mime (mime_type);
        if (!service_type) {
                return;
        }

        if (!tracker_ontology_service_has_metadata (service_type)) {
                return;
        }

        context = extractor_context_create (metadata);

	if (!context) {
		return;
	}

	g_object_set_data (G_OBJECT (file), "extractor-context", context);
        path = g_file_get_path (file);

        org_freedesktop_Tracker_Extract_get_metadata_async (get_dbus_extract_proxy (),
                                                            path,
                                                            mime_type,
                                                            get_metadata_async_cb,
                                                            context);

        g_main_loop_run (context->main_loop);

        g_object_set_data (G_OBJECT (file), "extractor-context", NULL);
        extractor_context_destroy (context);
        g_free (path);
}

static gboolean
get_file_content_read_cb (GIOChannel   *channel,
			  GIOCondition	condition,
			  gpointer	user_data)
{
	ProcessContext *context;
	GString *text;
	GIOStatus status;
	gchar *line;

	context = user_data;
	text = context->data;;
	status = G_IO_STATUS_NORMAL;

	if (condition & G_IO_IN || condition & G_IO_PRI) {
		do {
			GError *error = NULL;

			status = g_io_channel_read_line (channel, &line, NULL, NULL, &error);

			if (status == G_IO_STATUS_NORMAL) {
				g_string_append (text, line);
				g_free (line);
			} else if (error) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		} while (status == G_IO_STATUS_NORMAL);

		if (status == G_IO_STATUS_EOF ||
		    status == G_IO_STATUS_ERROR) {
			g_main_loop_quit (context->data_incoming_loop);
			return FALSE;
		}
	}

	if (condition & G_IO_ERR || condition & G_IO_HUP) {
		g_main_loop_quit (context->data_incoming_loop);
		return FALSE;
	}

	return TRUE;
}

static gboolean
get_file_is_utf8 (GString *s,
		  gssize  *bytes_valid)
{
	const gchar *end;

	/* Check for UTF-8 validity, since we may
	 * have cut off the end.
	 */
	if (g_utf8_validate (s->str, s->len, &end)) {
		*bytes_valid = (gssize) s->len;
		return TRUE;
	}

	*bytes_valid = end - s->str;

	/* 4 is the maximum bytes for a UTF-8 character. */
	if (*bytes_valid > 4) {
		return FALSE;
	}

	if (g_utf8_get_char_validated (end, *bytes_valid) == (gunichar) -1) {
		return FALSE;
	}

	return TRUE;
}

#ifdef TRY_LOCALE_TO_UTF8_CONVERSION

static GString *
get_file_in_locale (GString *s)
{
	GError *error = NULL;
	gchar  *str;
	gsize	bytes_read;
	gsize	bytes_written;

	str = g_locale_to_utf8 (s->str,
				s->len,
				&bytes_read,
				&bytes_written,
				&error);
	if (error) {
		g_debug ("  Conversion to UTF-8 read %d bytes, wrote %d bytes",
			 bytes_read,
			 bytes_written);
		g_message ("Could not convert file from locale to UTF-8, %s",
			   error->message);
		g_error_free (error);
		g_free (str);
	} else {
		g_string_assign (s, str);
		g_free (str);
	}

	return s;
}

#endif /* TRY_LOCALE_TO_UTF8_CONVERSION */

static gchar *
get_file_content (const gchar *path)
{
	GFile		 *file;
	GFileInputStream *stream;
	GError		 *error = NULL;
	GString		 *s;
	gssize		  bytes;
	gssize		  bytes_valid;
	gssize		  bytes_read_total;
	gssize		  buf_size;
	gchar		  buf[TEXT_CHECK_SIZE];
	gboolean	  has_more_data;
	gboolean	  has_reached_max;
	gboolean	  is_utf8;

	file = g_file_new_for_path (path);
	stream = g_file_read (file, NULL, &error);

	if (error) {
		g_message ("Could not get read file:'%s', %s",
			   path,
			   error->message);
		g_error_free (error);
		g_object_unref (file);

		return NULL;
	}

	s = g_string_new ("");
	has_reached_max = FALSE;
	has_more_data = TRUE;
	bytes_read_total = 0;
	buf_size = TEXT_CHECK_SIZE - 1;

	g_debug ("  Starting read...");

	while (has_more_data && !has_reached_max && !error) {
		gssize bytes_read;
		gssize bytes_remaining;

		/* Leave space for NULL termination and make sure we
		 * add it at the end now.
		 */
		bytes_remaining = buf_size;
		bytes_read = 0;

		/* Loop until we hit the maximum */
		for (bytes = -1; bytes != 0 && !error; ) {
			bytes = g_input_stream_read (G_INPUT_STREAM (stream),
						     buf,
						     bytes_remaining,
						     NULL,
						     &error);

			bytes_read += bytes;
			bytes_remaining -= bytes;

			g_debug ("  Read %" G_GSSIZE_FORMAT " bytes", bytes);
		}

		/* Set the NULL termination after the last byte read */
		buf[buf_size - bytes_remaining] = '\0';

		/* First of all, check if this is the first time we
		 * have tried to read the file up to the TEXT_CHECK_SIZE
		 * limit. Then make sure that we read the maximum size
		 * of the buffer. If we don't do this, there is the
		 * case where we read 10 bytes in and it is just one
		 * line with no '\n'. Once we have confirmed this we
		 * check that the buffer has a '\n' to make sure the
		 * file is worth indexing. Similarly if the file has
		 * <= 3 bytes then we drop it.
		 */
		if (bytes_read_total == 0) {
			if (bytes_read == buf_size &&
			    strchr (buf, '\n') == NULL) {
				g_debug ("  No '\\n' in the first %" G_GSSIZE_FORMAT " bytes, not indexing file",
					 buf_size);
				break;
			} else if (bytes_read <= 2) {
				g_debug ("  File has less than 3 characters in it, not indexing file");
				break;
			}
		}

		/* Here we increment the bytes read total to evaluate
		 * the next states. We don't do this before the
		 * previous condition so we can know when we have
		 * iterated > 1.
		 */
		bytes_read_total += bytes_read;

		if (bytes_read != buf_size || bytes_read == 0) {
			has_more_data = FALSE;
		}

		if (bytes_read_total >= TEXT_MAX_SIZE) {
			has_reached_max = TRUE;
		}

		g_debug ("  Read "
			 "%" G_GSSIZE_FORMAT " bytes total, "
			 "%" G_GSSIZE_FORMAT " bytes this time, "
			 "more data:%s, reached max:%s",
			 bytes_read_total,
			 bytes_read,
			 has_more_data ? "yes" : "no",
			 has_reached_max ? "yes" : "no");

		/* The + 1 is for the NULL terminating byte */
		s = g_string_append_len (s, buf, bytes_read + 1);
	}

	if (has_reached_max) {
		g_debug ("  Maximum indexable limit reached");
	}

	if (error) {
		g_message ("Could not read input stream for:'%s', %s",
			   path,
			   error->message);
		g_error_free (error);
		g_string_free (s, TRUE);
		g_object_unref (stream);
		g_object_unref (file);

		return NULL;
	}

	/* Check for UTF-8 Validity, if not try to convert it to the
	 * locale we are in.
	 */
	is_utf8 = get_file_is_utf8 (s, &bytes_valid);

	/* Make sure the string is NULL terminated and in the case
	 * where the string is valid UTF-8 up to the last character
	 * which was cut off, NULL terminate to the last most valid
	 * character.
	 */
#ifdef TRY_LOCALE_TO_UTF8_CONVERSION
	if (!is_utf8) {
		s = get_file_in_locale (s);
	} else {
		g_debug ("  Truncating to last valid UTF-8 character (%d/%d bytes)",
			 bytes_valid,
			 s->len);
		s = g_string_truncate (s, bytes_valid);
	}
#else	/* TRY_LOCALE_TO_UTF8_CONVERSION */
	g_debug ("  Truncating to last valid UTF-8 character (%" G_GSSIZE_FORMAT "/%" G_GSSIZE_FORMAT " bytes)",
		 bytes_valid,
		 s->len);
	s = g_string_truncate (s, bytes_valid);
#endif	/* TRY_LOCALE_TO_UTF8_CONVERSION */

	g_object_unref (stream);
	g_object_unref (file);

	if (s->len < 1) {
		g_string_free (s, TRUE);
		s = NULL;
	}

	return s ? g_string_free (s, FALSE) : NULL;
}

static gchar *
get_file_content_by_filter (GFile       *file,
			    const gchar *mime)
{
	ProcessContext *context;
	gchar *str, *text_filter_file;
	gchar **argv;
	GString *text;

#ifdef G_OS_WIN32
	str = g_strconcat (mime, "_filter.bat", NULL);
#else
	str = g_strconcat (mime, "_filter", NULL);
#endif

	text_filter_file = g_build_filename (LIBDIR,
					     "tracker",
					     "filters",
					     str,
					     NULL);

	g_free (str);

	if (!g_file_test (text_filter_file, G_FILE_TEST_EXISTS)) {
		g_free (text_filter_file);
		return NULL;
	}

	argv = g_new0 (gchar *, 3);
	argv[0] = text_filter_file;
	argv[1] = g_file_get_path (file);

	g_message ("Extracting text for:'%s' using filter:'%s'", argv[1], argv[0]);

	context = process_context_create ((const gchar **) argv,
					  get_file_content_read_cb);

	g_strfreev (argv);
	g_object_set_data (G_OBJECT (file), "text-filter-context", context);

	if (!context) {
		return NULL;
	}

	text = g_string_new (NULL);
	context->data = text;

	/* It will block here until all incoming
	 * text has been processed
	 */
	g_main_loop_run (context->data_incoming_loop);

	g_object_set_data (G_OBJECT (file), "text-filter-context", NULL);

	return g_string_free (text, FALSE);
}

/**
 * tracker_module_metadata_utils_get_text:
 * @file: A #GFile
 *
 * Gets the text from @file, if the file is considered as
 * containing plain text, it will be extracted, else this function
 * will resort to the installed text filters for the file MIME type.
 *
 * Returns: A newly allocated string containing the file text, or %NULL.
 **/
gchar *
tracker_module_metadata_utils_get_text (GFile *file)
{
	const gchar *service_type;
	gchar *path, *mime_type, *text;

	path = g_file_get_path (file);

	mime_type = tracker_file_get_mime_type (path);
	service_type = tracker_ontology_get_service_by_mime (mime_type);

	/* No need to filter text based files - index them directly */
	if (service_type &&
	    (strcmp (service_type, "Text") == 0 ||
	     strcmp (service_type, "Development") == 0)) {
		text = get_file_content (path);
	} else {
		text = get_file_content_by_filter (file, mime_type);
	}

	g_free (mime_type);
	g_free (path);

	return text;
}

/**
 * tracker_module_metadata_utils_get_data:
 * @file: A #GFile
 *
 * Returns a #TrackerModuleMetadata filled in with all the
 * metadata that could be extracted for the given file.
 *
 * Returns: A newly created #TrackerModuleMetadata, or %NULL if the file is not found.
 **/
TrackerModuleMetadata *
tracker_module_metadata_utils_get_data (GFile *file)
{
	TrackerModuleMetadata *metadata;
	GFileInfo             *info;
	gchar                 *path;
	const gchar           *mime_type;
	gchar                 *dirname, *basename;
	guint64                modified, accessed;
	GError                *error = NULL;

	info = g_file_query_info (file, 
				  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
				  G_FILE_ATTRIBUTE_STANDARD_SIZE ","
				  G_FILE_ATTRIBUTE_TIME_ACCESS "," 
				  G_FILE_ATTRIBUTE_TIME_MODIFIED,  
				  G_FILE_QUERY_INFO_NONE, 
				  NULL, 
				  &error);
	
	if (error) {
		g_warning ("Unable to retrieve info from file (%s)", error->message);
		return NULL;
	}

	path = g_file_get_path (file);
	dirname = g_path_get_dirname (path);
	basename = g_filename_display_basename (path);

	mime_type = g_file_info_get_content_type (info);
	modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	accessed = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);

	metadata = tracker_module_metadata_new ();

	tracker_module_metadata_add_string (metadata, 
					    METADATA_FILE_NAME, 
					    basename);
	tracker_module_metadata_add_string (metadata, 
					    METADATA_FILE_PATH, 
					    dirname);
	tracker_module_metadata_add_string (metadata, 
					    METADATA_FILE_MIMETYPE, 
					    mime_type ? mime_type : "unknown");
	tracker_module_metadata_add_offset (metadata, 
					    METADATA_FILE_SIZE, 
					    g_file_info_get_size (info));
	tracker_module_metadata_add_uint64 (metadata, 
					    METADATA_FILE_MODIFIED, 
					    modified);
	tracker_module_metadata_add_uint64 (metadata, 
					    METADATA_FILE_ACCESSED, 
					    accessed);
	tracker_module_metadata_add_date (metadata, METADATA_FILE_ADDED, time (NULL));

	metadata_utils_get_embedded (file, mime_type, metadata);


	g_free (basename);
	g_free (dirname);
	g_free (path);

	return metadata;
}

void
tracker_module_metadata_utils_cancel (GFile *file)
{
	ProcessContext *process_context;
	ExtractorContext *extractor_context;

	process_context = g_object_get_data (G_OBJECT (file), "text-filter-context");

	if (process_context) {
		process_context_kill (process_context);
	}

	extractor_context = g_object_get_data (G_OBJECT (file), "extractor-context");

	if (extractor_context) {
		extractor_context_kill (extractor_context);
	}
}
