/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * xesam-glib
 * Copyright (C) Mikkel Kamstrup Erlandsen 2007 <mikkel.kamstrup@gmail.com>
 *
 * xesam-glib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * xesam-glib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with xesam-glib.  If not, write to:
 *	The Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor
 *	Boston, MA  02110-1301, USA.
 */

/*
 * xesam-g-utils.h defines commonly used *private* functionality
 * of the xesam-glib.
 */

#ifndef _XESAM_G_UTILS_H_
#define _XESAM_G_UTILS_H_

#include <glib-object.h>

G_BEGIN_DECLS

GValue*			init_value_if_null				(GValue **value,
												 GType value_type);

gchar*			g_property_to_xesam_property	(gchar	*g_prop_name);

void			free_ptr_array_of_values		(GPtrArray *array);

GValue*			xesam_g_clone_value				(const GValue *orig);

const gchar*	digit_as_const_char				(guint			digit);

G_END_DECLS

#endif /* _XESAM_G_UTILS_H_ */
