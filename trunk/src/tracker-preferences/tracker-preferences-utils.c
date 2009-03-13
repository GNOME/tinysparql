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

#include "tracker-preferences-utils.h"

static const gchar *claws_binaries[] = {
	"claws-mail",
	"sylpheed-claws-gtk2",
	"sylpheed-claws",
	"sylpheed"
};

static const gchar *thunderbird_binaries[] = {
	"thunderbird",
	"mozilla-thunderbird"
};

static const gchar *evolution_binaries[] = {
	"evolution"
};

static const gchar *kmail_binaries[] = {
	"kmail"
};

gchar *
get_claws_command (void)
{
	guint i;
	gchar *cmd = NULL;

	for (i = 0; cmd == NULL && i < G_N_ELEMENTS (claws_binaries); i++)
		cmd = g_find_program_in_path (claws_binaries[i]);

	return cmd;
}

gchar *
get_thunderbird_command (void)
{
	guint i;
	gchar *cmd = NULL;

	for (i = 0; cmd == NULL && i < G_N_ELEMENTS (thunderbird_binaries);
	     i++)
		cmd = g_find_program_in_path (thunderbird_binaries[i]);

	return cmd;
}

gchar *
get_evolution_command (void)
{
	guint i;
	gchar *cmd = NULL;

	for (i = 0; cmd == NULL && i < G_N_ELEMENTS (evolution_binaries); i++)
		cmd = g_find_program_in_path (evolution_binaries[i]);

	return cmd;
}

gchar *
get_kmail_command (void)
{
	guint i;
	gchar *cmd = NULL;

	for (i = 0; cmd == NULL && i < G_N_ELEMENTS (kmail_binaries); i++)
		cmd = g_find_program_in_path (kmail_binaries[i]);

	return cmd;
}

gboolean
evolution_available (void)
{
	gchar *command = get_evolution_command ();

	if (!command)
		return FALSE;

	g_free (command);
	return TRUE;
}

gboolean
thunderbird_available (void)
{
	gchar *command = get_thunderbird_command ();

	if (!command)
		return FALSE;

	g_free (command);
	return TRUE;
}

gboolean
kmail_available (void)
{
	gchar *command = get_kmail_command ();

	if (!command)
		return FALSE;

	g_free (command);
	return TRUE;
}

gboolean
convert_available (void)
{
	gchar *command = g_find_program_in_path ("convert");

	if (!command)
		return FALSE;

	g_free (command);
	return TRUE;
}
