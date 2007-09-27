#include "tm-debug.h"

void
critical_error(const gchar *format, ...)
{
   va_list args;
   gchar *title = NULL;
   gchar *message = NULL;
   GtkWidget *dialog = NULL;

   va_start(args, format);
   message = g_strdup_vprintf(format, args);
   va_end(args);

   title = g_strdup_printf("<span size='large'><b>%s</b></span>", PROGRAM_NAME);

   dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING, GTK_BUTTONS_CLOSE, NULL);
   gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), title);
   gtk_message_dialog_format_secondary_markup(GTK_MESSAGE_DIALOG(dialog), message);

   gtk_dialog_run(GTK_DIALOG(dialog));

   g_free(title);
   g_free(message);
   gtk_widget_destroy(dialog);
}
