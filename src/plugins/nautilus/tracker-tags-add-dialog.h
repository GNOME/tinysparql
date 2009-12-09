/*
 * Copyright (C) 2009  Debarshi Ray <debarshir@src.gnome.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef TRACKER_TAGS_ADD_DIALOG_H
#define TRACKER_TAGS_ADD_DIALOG_H

#include <gtk/gtk.h>

#define TRACKER_TYPE_TAGS_ADD_DIALOG (tracker_tags_add_dialog_get_type ())
#define TRACKER_TAGS_ADD_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), TRACKER_TYPE_TAGS_ADD_DIALOG, TrackerTagsAddDialog))
#define TRACKER_TAGS_ADD_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), TRACKER_TYPE_TAGS_ADD_DIALOG, TrackerTagsAddDialogClass))
#define TRACKER_IS_TAGS_ADD_DIALOG(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TRACKER_TYPE_TAGS_ADD_DIALOG))
#define TRACKER_IS_TAGS_ADD_DIALOG_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TRACKER_TYPE_TAGS_ADD_DIALOG))

typedef struct _TrackerTagsAddDialogPrivate	TrackerTagsAddDialogPrivate;

typedef struct _TrackerTagsAddDialog		TrackerTagsAddDialog;
typedef struct _TrackerTagsAddDialogClass	TrackerTagsAddDialogClass;

struct _TrackerTagsAddDialog
{
	GtkDialog parent;
	TrackerTagsAddDialogPrivate *priv;
};

struct _TrackerTagsAddDialogClass
{
	GtkDialogClass parent;
};

GType	tracker_tags_add_dialog_get_type	(void);
void	tracker_tags_add_dialog_register_type	(GTypeModule *module);

GtkWidget	*tracker_tags_add_dialog_new		(void);
const gchar	*tracker_tags_add_dialog_get_text	(TrackerTagsAddDialog *add_dialog);

#endif /* TRACKER_TAGS_ADD_DIALOG_H */
