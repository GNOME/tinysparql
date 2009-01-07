/* Copyright (C) 2006, Mr Jamie McCracken (jamiemcc@gnome.org)
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

#include "tracker-module-iteratable.h"
#include "tracker-module-file.h"

GType
tracker_module_iteratable_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                type = g_type_register_static_simple (G_TYPE_INTERFACE,
                                                      "TrackerModuleIteratable",
                                                      sizeof (TrackerModuleIteratableIface),
                                                      NULL, 0, NULL, 0);

                g_type_interface_add_prerequisite (type, TRACKER_TYPE_MODULE_FILE);
        }

        return type;
}

/**
 * tracker_module_iteratable_iter_contents:
 * @iteratable: A #TrackerModuleIteratable
 *
 * Iterates to the next element contained in @iteratable.
 *
 * Returns: %TRUE if there was such next element, %FALSE otherwise
 **/
gboolean
tracker_module_iteratable_iter_contents (TrackerModuleIteratable *iteratable)
{
        return TRACKER_MODULE_ITERATABLE_GET_IFACE (iteratable)->iter_contents (iteratable);
}

/**
 * tracker_module_iteratable_get_count:
 * @iteratable: A #TrackerModuleIteratable
 *
 * Returns the number of elements contained in @iteratable
 *
 * Returns: The elements count.
 **/
guint
tracker_module_iteratable_get_count (TrackerModuleIteratable *iteratable)
{
        if (TRACKER_MODULE_ITERATABLE_GET_IFACE (iteratable)->get_count) {
                return TRACKER_MODULE_ITERATABLE_GET_IFACE (iteratable)->get_count (iteratable);
        }

        return 1;
}
