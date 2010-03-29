/*
 * Copyright (C) 2009, Nokia
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

#ifndef __TTL_QNAME_H__
#define __TTL_QNAME_H__

#include <glib.h>

G_BEGIN_DECLS

void     qname_init          (const gchar *local_uri,
                              const gchar *local_prefix,
                              const gchar *class_location);
void     qname_shutdown      (void);

gchar *  qname_to_link       (const gchar *qname);
gchar *  qname_to_shortname  (const gchar *qname);
gchar *  qname_to_classname  (const gchar *qname);


gboolean qname_is_basic_type (const gchar *qname);


G_END_DECLS

#endif
