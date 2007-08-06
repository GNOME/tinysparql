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

#ifndef __TRACKER_CONFIGURATION_H__
#define __TRACKER_CONFIGURATION_H__

#include <glib-object.h>

G_BEGIN_DECLS
#define TRACKER_TYPE_CONFIGURATION              (tracker_configuration_get_type())
#define TRACKER_CONFIGURATION(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), TRACKER_TYPE_CONFIGURATION, TrackerConfiguration))
#define TRACKER_CONFIGURATION_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), TRACKER_TYPE_CONFIGURATION, TrackerConfigurationClass))
#define TRACKER_IS_CONFIGURATION(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), TRACKER_TYPE_CONFIGURATION))
#define TRACKER_IS_CONFIGURATION_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), TRACKER_TYPE_CONFIGURATION))
#define TRACKER_CONFIGURATION_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), TRACKER_TYPE_CONFIGURATION, TrackerConfigurationClass))

typedef struct _TrackerConfiguration {
	GObject parent;
} TrackerConfiguration;

typedef struct _TrackerConfigurationClass {
	GObjectClass parent_class;

	/* Methods */
	void (*write) (TrackerConfiguration * configuration);

	gboolean (*get_bool) (TrackerConfiguration * configuration,
				const gchar * const key, GError ** error);
	void (*set_bool) (TrackerConfiguration * configuration,
			  const gchar * const key, const gboolean value);

	gint (*get_int) (TrackerConfiguration * configuration,
			   const gchar * const key, GError ** error);
	void (*set_int) (TrackerConfiguration * configuration,
			 const gchar * const key, const gint value);

	gchar *(*get_string) (TrackerConfiguration * configuration,
			      const gchar * const key, GError ** error);
	void (*set_string) (TrackerConfiguration * configuration,
			    const gchar * const key,
			    const gchar * const value);

	GSList *(*get_list) (TrackerConfiguration * configuration,
			     const gchar * const key, GType g_type,
			     GError ** error);
	void (*set_list) (TrackerConfiguration * configuration,
			  const gchar * const key, const GSList * const list,
			  GType g_type);
} TrackerConfigurationClass;

GType
tracker_configuration_get_type (void);

TrackerConfiguration *
tracker_configuration_new (void);

void
tracker_configuration_write (TrackerConfiguration * configuration);

gboolean
tracker_configuration_get_bool (TrackerConfiguration * configuration,
				const gchar * const key, GError ** error);

void
tracker_configuration_set_bool (TrackerConfiguration * configuration,
				const gchar * const key, const gboolean value);

gint
tracker_configuration_get_int (TrackerConfiguration * configuration,
			       const gchar * const key, GError ** error);

void
tracker_configuration_set_int (TrackerConfiguration * configuration,
			       const gchar * const key, const gint value);

gchar *
tracker_configuration_get_string (TrackerConfiguration * configuration,
				  const gchar * const key,
				  GError ** error);

void
tracker_configuration_set_string (TrackerConfiguration * configuration,
				  const gchar * const key, const gchar * const value);

GSList *
tracker_configuration_get_list (TrackerConfiguration * configuration,
				const gchar * const key, GType g_type,
				GError ** error);

void
tracker_configuration_set_list (TrackerConfiguration * configuration,
				const gchar * const key, const GSList * const value,
				GType g_type);

typedef struct {
	gchar *lang;
	gchar *name;
} Matches;


extern Matches tmap[];

G_END_DECLS
#endif
