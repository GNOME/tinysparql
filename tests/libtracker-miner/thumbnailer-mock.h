#ifndef __THUMBNAILER_MOCK_H__
#define __THUMBNAILER_MOCK_H__

#include <glib.h>

G_BEGIN_DECLS

void    dbus_mock_call_log_reset (void);
GList * dbus_mock_call_log_get   (void);

G_END_DECLS


#endif
