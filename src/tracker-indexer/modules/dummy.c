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

#include <glib.h>
#include <tracker-indexer/tracker-metadata.h>

void
tracker_module_init (void)
{
	/* Implementing this function is optional.
	 *
	 * Allocate here all static resources for the module.
	 */
}

void
tracker_module_shutdown (void)
{
	/* Implementing this function is optional.
	 *
	 * Free here all resources allocated in tracker_module_init()
	 */
}

G_CONST_RETURN gchar *
tracker_module_get_name (void)
{
	/* Return module name here */
	return "Dummy";
}

gpointer
tracker_module_file_get_data (const gchar *path)
{
	/* Implementing this function is optional.
	 *
	 * Return here private, module specific data for path.
	 * Given this data is attached to the file until it isn't
	 * needed anymore. This is usually used for files that
	 * contain sets of data that should be considered as separate
	 * entities (for example, mboxes), so the module can internally
	 * keep the state. Also see tracker_module_file_iter_contents().
	 */
	return NULL;
}

gchar *
tracker_module_file_get_service_type (TrackerFile *file)
{
	/* Implementing this function is optional.
	 *
	 * Return the service type for the incoming path.
	 *
	 * If this function is not implemented, the indexer will use
	 * the name of the module (tracker_module_get_name) as service.
	 *
	 */
	return NULL;
}

void
tracker_module_file_free_data (gpointer file_data)
{
	/* Implementing this function is optional
	 *
	 * Free the data created previously
	 * through tracker_module_file_get_data()
	 */
}

void
tracker_module_file_get_uri (TrackerFile  *file,
			     gchar	 **dirname,
			     gchar	 **basename)
{
	/* Implementing this function is optional
	 *
	 * Return dirname/basename for the current item, with this
	 * method modules can specify different URIs for different
	 * elements contained in the file. Also see
	 * tracker_module_file_iter_contents()
	 */
	*dirname = g_path_get_dirname (file->path);
	*basename = g_path_get_basename (file->path);
}

TrackerMetadata *
tracker_module_file_get_metadata (TrackerFile *file)
{
	/* Return a hashtable filled with metadata for file, given the
	 * current state. Also see tracker_module_file_iter_contents()
	 */
	return NULL;
}

gchar *
tracker_module_file_get_text (TrackerFile *file)
{
	/* Implementing this function is optional
	 *
	 * Return here full text for file, given the current state,
	 * also see tracker_module_file_iter_contents()
	 */
	return NULL;
}

gboolean
tracker_module_file_iter_contents (TrackerFile *file)
{
	/* Implementing this function is optional
	 *
	 * This function is meant to iterate the internal state,
	 * so it points to the next entity inside the file.
	 * In case there is such next entity, this function must
	 * return TRUE, else, returning FALSE will make the indexer
	 * think it is done with this file and move on to the next one.
	 *
	 * What an "entity" is considered is left to the module
	 * implementation.
	 */
	return FALSE;
}
