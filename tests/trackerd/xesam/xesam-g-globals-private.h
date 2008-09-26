/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with main.c; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

/*
 * This file contains declarations that are considered to be "package private"
 * in Java terminology. Ie methods and constants for shared use inside
 * xesam-glib only.
 */

#ifndef _XESAM_G_GLOBALS_PRIVATE_H_
#define _XESAM_G_GLOBALS_PRIVATE_H_

#include <xesam-glib/xesam-g-search.h>
#include <xesam-glib/xesam-g-hit.h>
#include <xesam-glib/xesam-g-hits.h>

G_BEGIN_DECLS

XesamGSearch* xesam_g_search_new		    (XesamGSession	*session,
													 XesamGQuery	*query);

XesamGSearch* xesam_g_search_new_from_text	    (XesamGSession	*session,
													 gchar			*search_text);

XesamGHit*    xesam_g_hit_new			    (guint	    id,
						     GHashTable     *field_map,
						     GPtrArray	    *field_data);

XesamGHits*   xesam_g_hits_new			    (XesamGSearch   *search,
						     guint	    batch_offset,
						     guint	    count,
						     GPtrArray	    *hits_data);

GHashTable*   xesam_g_session_get_field_map	    (XesamGSession  *session);

GHashTable*   xesam_g_search_get_field_map	    (XesamGSearch  *search);



G_END_DECLS

#endif /* _XESAM_G_GLOBALS_PRIVATE_H_ */
