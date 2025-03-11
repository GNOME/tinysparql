/* This is the source code of our Coverity model. Meant to enhance
 * the accuracy of reports, improve understanding of the existing
 * code, or avoid false positives.
 *
 * This is not a real C file, and can not use #include directives.
 * These are basically hints about the function behavior, in a way
 * that Coverity can understand.
 *
 * The model file must be uploaded by someone with permissions at the
 * Tinysparql console at https://scan.coverity.com/projects/tracker
 */

#define NULL ((void *) 0)

typedef long ssize_t;
typedef struct _GObject { int ref_count } GObject;
typedef void (*GDestroyNotify) (void *data);
typedef struct _GInputStream { GObject parent; void *data; GDestroyNotify free } GInputStream;
typedef struct _GMappedFile { char *data } GMappedFile;
typedef unsigned int gboolean;

GInputStream *
g_memory_input_stream_new_from_data (void           *data,
                                     ssize_t         len,
                                     GDestroyNotify  destroy_notify)
{
	if (len < 0)
		__coverity_string_null_sink__ (data);
	if (destroy_notify)
		__coverity_escape__ (data);
	return __coverity_alloc__ (sizeof(GInputStream));
}

char *
g_mapped_file_get_contents (GMappedFile *mapped_file)
{
	return mapped_file->data;
}

char *
g_strcanon (char *string, char *valid_chars, char substitutor)
{
	__coverity_string_null_sink__ (string);
	__coverity_string_null_sink__ (valid_chars);
	return string;
}

char *
g_strchug (char *string)
{
	__coverity_string_null_sink__ (string);
	return string;
}

char *
g_strchomp (char *string)
{
	__coverity_string_null_sink__ (string);
	return string;
}

char *
g_strdelimit (char *string, char *delimiters, char new_delimiter)
{
	__coverity_string_null_sink__ (string);
	__coverity_string_null_sink__ (delimiters);
	return string;
}

gboolean
g_utf8_validate (char *string, ssize_t size, const char **end)
{
	int is_ok;

	if (is_ok) {
		__coverity_tainted_string_sanitize_content__ (string);
		return 1;
	} else {
		return 0;
	}
}
