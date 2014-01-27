/*
 * Copyright (C) 2014 Carlos Garnacho  <carlosg@gnome.org>
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

#ifndef __LIBTRACKER_MINER_DECORATOR_INTERNAL_H__
#define __LIBTRACKER_MINER_DECORATOR_INTERNAL_H__

#include "tracker-decorator.h"

G_BEGIN_DECLS

void _tracker_decorator_query_append_rdf_type_filter (TrackerDecorator *decorator,
                                                      GString          *query);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_DECORATOR_INTERNAL_H__ */
