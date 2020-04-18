/*
 * Copyright (C) 2017, Red Hat, Inc
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef __TTLRESOURCE2XML_H__
#define __TTLRESOURCE2XML_H__

#include <glib.h>
#include "ttl_model.h"

G_BEGIN_DECLS

void print_ontology_class (Ontology      *ontology,
                           OntologyClass *klass,
                           FILE          *f);
void print_ontology_extra_properties (Ontology      *ontology,
                                      const char    *ontology_prefix,
                                      const char    *classname,
                                      GList         *properties_for_class,
                                      FILE          *f);

G_END_DECLS

#endif /* __TTLRESOURCE2XML__ */
