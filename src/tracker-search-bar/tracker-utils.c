/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "config.h"

#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <regex.h>

#include <glib/gi18n.h>

#include "tracker-utils.h"

/*
 * Regular Expression code to match urls.
 */
#define USERCHARS "-A-Za-z0-9"
#define PASSCHARS "-A-Za-z0-9,?;.:/!%$^*&~\"#'"
#define HOSTCHARS "-A-Za-z0-9"
#define PATHCHARS "-A-Za-z0-9_$.+!*(),;:@&=?/~#%"
#define SCHEME    "(news:|telnet:|nntp:|file:/|https?:|ftps?:|webcal:)"
#define USER      "[" USERCHARS "]+(:["PASSCHARS "]+)?"
#define URLPATH   "/[" PATHCHARS "]*[^]'.}>) \t\r\n,\\\"]"

static regex_t dingus[TRACKER_REGEX_ALL];

static void
regex_init (void)
{
	static gboolean  inited = FALSE;
	const gchar     *expression;
	gint             i;

	if (inited) {
		return;
	}

	for (i = 0; i < TRACKER_REGEX_ALL; i++) {
		switch (i) {
		case TRACKER_REGEX_AS_IS:
			expression =
				SCHEME "//(" USER "@)?[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case TRACKER_REGEX_BROWSER:
			expression =
				"(www|ftp)[" HOSTCHARS "]*\\.[" HOSTCHARS ".]+"
				"(:[0-9]+)?(" URLPATH ")?";
			break;
		case TRACKER_REGEX_EMAIL:
			expression =
				"(mailto:)?[a-z0-9][a-z0-9.-]*@[a-z0-9]"
				"[a-z0-9-]*(\\.[a-z0-9][a-z0-9-]*)+";
			break;
		case TRACKER_REGEX_OTHER:
			expression =
				"news:[-A-Z\\^_a-z{|}~!\"#$%&'()*+,./0-9;:=?`]+"
				"@[" HOSTCHARS ".]+(:[0-9]+)?";
			break;
		default:
			/* Silence the compiler. */
			expression = NULL;
			continue;
		}

		memset (&dingus[i], 0, sizeof (regex_t));
		regcomp (&dingus[i], expression, REG_EXTENDED | REG_ICASE);
	}

	inited = TRUE;
}

gint
tracker_regex_match (TrackerRegExType  type,
                     const gchar      *msg,
                     GArray           *start,
                     GArray           *end)
{
	regmatch_t matches[1];
	gint       ret = 0;
	gint       num_matches = 0;
	gint       offset = 0;
	gint       i;

	g_return_val_if_fail (type <= TRACKER_REGEX_ALL, 0);

	regex_init ();

	while (!ret && type != TRACKER_REGEX_ALL) {
		ret = regexec (&dingus[type], msg + offset, 1, matches, 0);
		if (ret == 0) {
			gint s;

			num_matches++;

			s = matches[0].rm_so + offset;
			offset = matches[0].rm_eo + offset;

			if (start) {
				g_array_append_val (start, s);
			}

			if (end) {
				g_array_append_val (end, offset);
			}
		}
	}

	if (type != TRACKER_REGEX_ALL) {
		/* g_debug ("Found %d matches for regex type:%d", */
		/*          num_matches, type); */
		return num_matches;
	}

	/* If TRACKER_REGEX_ALL then we run ALL regex's on the string. */
	for (i = 0; i < TRACKER_REGEX_ALL; i++, ret = 0) {
		while (!ret) {
			ret = regexec (&dingus[i], msg + offset, 1, matches, 0);
			if (ret == 0) {
				gint s;

				num_matches++;

				s = matches[0].rm_so + offset;
				offset = matches[0].rm_eo + offset;

				if (start) {
					g_array_append_val (start, s);
				}

				if (end) {
					g_array_append_val (end, offset);
				}
			}
		}
	}

	/* g_debug ("Found %d matches for ALL regex types", */
	/*          num_matches); */

	return num_matches;
}
