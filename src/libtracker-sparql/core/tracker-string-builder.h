/*
 * Copyright (C) 2008-2010, Nokia
 * Copyright (C) 2017-2018, Carlos Garnacho
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#pragma once

#include <glib.h>

typedef struct _TrackerStringBuilder TrackerStringBuilder;

TrackerStringBuilder * tracker_string_builder_new  (void);
void                   tracker_string_builder_free (TrackerStringBuilder *builder);

TrackerStringBuilder * tracker_string_builder_append_placeholder  (TrackerStringBuilder *builder);
TrackerStringBuilder * tracker_string_builder_prepend_placeholder (TrackerStringBuilder *builder);

void tracker_string_builder_append  (TrackerStringBuilder *builder,
                                     const gchar          *string,
                                     gssize                len);
void tracker_string_builder_prepend (TrackerStringBuilder *builder,
                                     const gchar          *string,
                                     gssize                len);
void tracker_string_builder_append_valist  (TrackerStringBuilder *builder,
                                            const gchar          *format,
                                            va_list               args);
void tracker_string_builder_append_printf  (TrackerStringBuilder *builder,
                                            const gchar          *format,
                                            ...);

gchar * tracker_string_builder_to_string (TrackerStringBuilder *builder);

gboolean tracker_string_builder_is_empty (TrackerStringBuilder *builder);
