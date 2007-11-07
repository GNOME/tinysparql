#ifndef __TM_TRAY_ICON_H__
#define __TM_TRAY_ICON_H__

#include <glib.h>
#include <glib-object.h>

#include <gtk/gtkstock.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkstatusicon.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtkseparatormenuitem.h>

#define TYPE_TRAY_ICON              (tray_icon_get_type())
#define TRAY_ICON(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_TRAY_ICON, TrayIcon))
#define TRAY_ICON_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_TRAY_ICON, TrayIconClass))
#define IS_TRAY_ICON(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_TRAY_ICON))
#define IS_TRAY_ICON_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_TRAY_ICON))
#define TRAY_ICON_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_TRAY_ICON, TrayIconClass))

typedef struct _TrayIcon
{
   GObject parent;
} TrayIcon;

typedef struct _TrayIconClass
{
   GObjectClass parent_class;
} TrayIconClass;

GType
tray_icon_get_type (void);

void
tray_icon_set_tooltip (TrayIcon *icon, const gchar *format, ...);

void 
tray_icon_show_message (TrayIcon *icon, const gchar *message, ...);

#endif
