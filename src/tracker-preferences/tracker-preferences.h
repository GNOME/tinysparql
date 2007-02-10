#ifndef __TRACKER_PREFERENCES_H__
#define __TRACKER_PREFERENCES_H__

#include <string.h>

/* TODO :: Include only needed headers */
#include <gtk/gtk.h>
#include <glib-object.h>

#include "tracker-configuration.h"
#include "tracker-preferences-utils.h"
#include "tracker-preferences-dialogs.h"

G_BEGIN_DECLS
#define TRACKER_TYPE_PREFERENCES             (tracker_preferences_get_type())
#define TRACKER_PREFERENCES(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferences))
#define TRACKER_PREFERENCES_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), TRACKER_TYPE_PREFERENCES, TrackerPreferencesClass))
#define TRACKER_IS_PREFERENCES(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), TRACKER_TYPE_PREFERENCES))
#define TRACKER_IS_PREFERENCES_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), TRACKER_TYPE_PREFERENCES))
#define TRACKER_PREFERENCES_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), TRACKER_TYPE_PREFERENCES, TrackerPreferencesClass))

typedef struct _TrackerPreferences {
	GObject parent;
} TrackerPreferences;

typedef struct _TrackerPreferencesClass {
	GObjectClass parent_class;
} TrackerPreferencesClass;

GType
tracker_preferences_get_type (void);

TrackerPreferences *
tracker_preferences_new (void);

G_END_DECLS
#endif
