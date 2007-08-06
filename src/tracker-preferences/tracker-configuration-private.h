/* Tracker - indexer and metadata database engine
 * Copyright (C) 2007, Saleem Abdulrasool (compnerd@gentoo.org)
 *
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

#ifndef __TRACKER_CONFIGURATION_PRIVATE_H__
#define __TRACKER_CONFIGURATION_PRIVATE_H__

#define TRACKER_CONFIGURATION_GET_PRIVATE(obj)  (G_TYPE_INSTANCE_GET_PRIVATE((obj), TRACKER_TYPE_CONFIGURATION, TrackerConfigurationPrivate))

typedef struct _TrackerConfigurationPrivate {
	gboolean dirty;
	gchar *filename;
	GKeyFile *keyfile;
} TrackerConfigurationPrivate;

static void
tracker_configuration_class_init (TrackerConfigurationClass * klass);

static void
tracker_configuration_init (GTypeInstance * instance, gpointer data);

static void
tracker_configuration_finalize (GObject * object);

static void
_write (TrackerConfiguration * configuration);

static gboolean
_get_bool (TrackerConfiguration * configuration, const gchar * const key,
	   GError ** error);

static void
_set_bool (TrackerConfiguration * configuration, const gchar * const key,
	   const gboolean value);

static gint
_get_int (TrackerConfiguration * configuration, const gchar * const key,
	  GError ** error);

static void
_set_int (TrackerConfiguration * configuration, const gchar * const key,
	  const gint value);

static gchar *
_get_string (TrackerConfiguration * configuration,
	     const gchar * const key, GError ** error);

static void
_set_string (TrackerConfiguration * configuration, const gchar * const key,
	     const gchar * const value);

static GSList *
_get_list (TrackerConfiguration * configuration,
	   const gchar * const key, GType type,
	   GError ** error);

static void
_set_list (TrackerConfiguration * configuration, const gchar * const key,
	   const GSList * const value, GType type);

static GSList *
_get_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, GError ** error);

static GSList *
_get_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error);

static GSList *
_get_int_list (TrackerConfiguration * configuration,
	       const gchar * const key, GError ** error);

static GSList *
_get_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, GError ** error);

static void
_set_boolean_list (TrackerConfiguration * configuration,
		   const gchar * const key, const GSList * const value);

static void
_set_double_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value);

static void
_set_int_list (TrackerConfiguration * configuration, const gchar * const key,
	       const GSList * const value);

static void
_set_string_list (TrackerConfiguration * configuration,
		  const gchar * const key, const GSList * const value);

#endif
