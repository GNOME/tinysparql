#ifndef __TM_DIALOGS_H__
#define __TM_DIALOGS_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkaboutdialog.h>
#include <gtk/gtkmessagedialog.h>

#include "tm-common.h"

void
critical_error(const gchar *format, ...);

#endif
