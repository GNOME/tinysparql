/*
 * Copyright (C) 2009, Nokia <ivan.frade@nokia.com>
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

#ifndef __LIBTRACKER_MINER_OBJECT_H__
#define __LIBTRACKER_MINER_OBJECT_H__

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <libtracker-sparql/tracker-sparql.h>

G_BEGIN_DECLS

/* Common definitions for all miners */
/**
 * TRACKER_MINER_DBUS_INTERFACE:
 *
 * The name of the D-Bus interface to use for all data miners that
 * inter-operate with Tracker.
 *
 * Since: 0.8
 **/
#define TRACKER_MINER_DBUS_INTERFACE   "org.freedesktop.Tracker1.Miner"

/**
 * TRACKER_MINER_DBUS_NAME_PREFIX:
 *
 * D-Bus name prefix to use for all data miners. This allows custom
 * miners to be written using @TRACKER_MINER_DBUS_NAME_PREFIX + "Files" for
 * example and would show up on D-Bus under
 * &quot;org.freedesktop.Tracker1.Miner.Files&quot;.
 *
 * Since: 0.8
 **/
#define TRACKER_MINER_DBUS_NAME_PREFIX "org.freedesktop.Tracker1.Miner."

/**
 * TRACKER_MINER_DBUS_PATH_PREFIX:
 *
 * D-Bus path prefix to use for all data miners. This allows custom
 * miners to be written using @TRACKER_MINER_DBUS_PATH_PREFIX + "Files" for
 * example and would show up on D-Bus under
 * &quot;/org/freedesktop/Tracker1/Miner/Files&quot;.
 *
 * Since: 0.8
 **/
#define TRACKER_MINER_DBUS_PATH_PREFIX "/org/freedesktop/Tracker1/Miner/"

#define TRACKER_TYPE_MINER         (tracker_miner_get_type())
#define TRACKER_MINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER, TrackerMiner))
#define TRACKER_MINER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER, TrackerMinerClass))
#define TRACKER_IS_MINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER))
#define TRACKER_IS_MINER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER))
#define TRACKER_MINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER, TrackerMinerClass))

/**
 * TRACKER_MINER_ERROR_DOMAIN:
 *
 * Used as the domain for any #GErrors reported by @TrackerMiner objects.
 *
 * Since: 0.8
 **/
#define TRACKER_MINER_ERROR_DOMAIN "TrackerMiner"

/**
 * TRACKER_MINER_ERROR:
 *
 * Returns the @GQuark used for #GErrors and for @TrackerMiner
 * implementations. This calls tracker_miner_error_quark().
 *
 * Since: 0.8
 **/
#define TRACKER_MINER_ERROR        tracker_miner_error_quark()

typedef struct _TrackerMiner TrackerMiner;
typedef struct _TrackerMinerPrivate TrackerMinerPrivate;

/**
 * TrackerMiner:
 *
 * Abstract miner object.
 **/
struct _TrackerMiner {
	GObject parent_instance;
	TrackerMinerPrivate *priv;
};

/**
 * TrackerMinerClass:
 * @parent_class: parent object class.
 * @started: Called when the miner is told to start collecting data.
 * @stopped: Called when the miner is told to stop collecting data.
 * @paused: Called when the miner is told to pause.
 * @resumed: Called when the miner is told to resume activity.
 * @progress: progress.
 * @padding: Reserved for future API improvements.
 *
 * Virtual methods left to implement.
 **/
typedef struct {
	GObjectClass parent_class;

	/* signals */
	void (* started)            (TrackerMiner *miner);
	void (* stopped)            (TrackerMiner *miner);

	void (* paused)             (TrackerMiner *miner);
	void (* resumed)            (TrackerMiner *miner);

	void (* progress)           (TrackerMiner *miner,
	                             const gchar  *status,
	                             gdouble       progress,
	                             gint          remaining_time);

	/* <Private> */
	gpointer padding[10];
} TrackerMinerClass;

/**
 * TrackerMinerError:
 * @TRACKER_MINER_ERROR_NAME_MISSING: No name was given when creating
 * the miner. The name is crucial for D-Bus presence and a host of
 * other things.
 * @TRACKER_MINER_ERROR_NAME_UNAVAILABLE: The name trying to be used
 * for the miner was not available, possibly because the miner is
 * already running with the same name in another process.
 * @TRACKER_MINER_ERROR_PAUSED: Given by miners when an API is used at
 * the time the miner itself is paused and such actions should be avoided.
 * @TRACKER_MINER_ERROR_PAUSED_ALREADY: The pause request has already
 * been given by the same application with the same reason. Duplicate
 * pause calls with the same reason by the same application can not
 * be carried out.
 * @TRACKER_MINER_ERROR_INVALID_COOKIE: When pausing a miner, a cookie
 * (or @gint based ID) is given. That cookie must be used to resume a
 * previous pause request. If the cookie is unrecognised, this error
 * is given.
 *
 * Possible errors returned when calling #TrackerMiner APIs or
 * subclassed miners where the error is generic to all miners.
 **/
typedef enum {
	TRACKER_MINER_ERROR_NAME_MISSING,
	TRACKER_MINER_ERROR_NAME_UNAVAILABLE,
	TRACKER_MINER_ERROR_PAUSED,
	TRACKER_MINER_ERROR_PAUSED_ALREADY,
	TRACKER_MINER_ERROR_INVALID_COOKIE
} TrackerMinerError;


GType                    tracker_miner_get_type            (void) G_GNUC_CONST;
GQuark                   tracker_miner_error_quark         (void);

void                     tracker_miner_start               (TrackerMiner         *miner);
void                     tracker_miner_stop                (TrackerMiner         *miner);
gboolean                 tracker_miner_is_started          (TrackerMiner         *miner);
gboolean                 tracker_miner_is_paused           (TrackerMiner         *miner);

void                     tracker_miner_pause               (TrackerMiner         *miner);
gboolean                 tracker_miner_resume              (TrackerMiner         *miner);

TrackerSparqlConnection *tracker_miner_get_connection      (TrackerMiner         *miner);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_OBJECT_H__ */
