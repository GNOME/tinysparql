/*
 * inotify-listhash.c - a structure to map wd's to client-side watches
 * Copyright © 2005 Ryan Lortie <desrt@desrt.ca>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#include <glib.h>

#include "inotify-handle.h"

#include "inotify-listhash.h"

static GHashTable *inotify_wd_table;

GSList *
inotify_listhash_get (gint32 wd)
{
        GSList *list;

        list = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));

        return list;
}

int
inotify_listhash_remove (INotifyHandle *inh)
{
        GSList *list;
        gint32 wd;

        wd = inotify_handle_get_wd (inh);

        list = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));

        if (list == NULL)
                return -1;

        list = g_slist_remove (list, inh);
        inotify_handle_unref (inh);

        if (list != NULL)
                g_hash_table_replace (inotify_wd_table, GINT_TO_POINTER (wd), list);
        else
                g_hash_table_remove (inotify_wd_table, GINT_TO_POINTER (wd));

        return g_slist_length (list);
}

void
inotify_listhash_append (INotifyHandle *inh, gint32 wd)
{
        GSList *list;

        inotify_handle_ref (inh);
        inotify_handle_set_wd (inh, wd);

        list = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));
        list = g_slist_append (list, inh);
        g_hash_table_replace (inotify_wd_table, GINT_TO_POINTER (wd), list);
}

int
inotify_listhash_ignore (gint32 wd)
{
        GSList *link, *next;

        link = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));
        g_hash_table_remove (inotify_wd_table, GINT_TO_POINTER (wd));

        if (link == NULL)
                return -1;

        for (; link; link = next)
        {
                next = link->next;

                inotify_handle_unref (link->data);
                g_slist_free_1 (link);
        }

        return 0;
}

int
inotify_listhash_length (gint32 wd)
{
        GSList *list;

        list = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));

        return g_slist_length (list);
}

guint32
inotify_listhash_get_mask (gint32 wd)
{
        GSList *list;
        guint32 mask;

        list = g_hash_table_lookup (inotify_wd_table, GINT_TO_POINTER (wd));

        for (mask = 0; list; list = list->next)
                mask |= inotify_handle_get_mask (list->data);

        return mask;
}

void
inotify_listhash_initialise (void)
{
        inotify_wd_table = g_hash_table_new (NULL, NULL);
}

void
inotify_listhash_destroy (void)
{
        g_hash_table_destroy (inotify_wd_table);
}

