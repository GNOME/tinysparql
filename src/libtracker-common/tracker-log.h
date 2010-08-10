/*
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef __LIBTRACKER_COMMON_LOG_H__
#define __LIBTRACKER_COMMON_LOG_H__

#include <glib.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_COMMON_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <libtracker-common/tracker-common.h> must be included directly."
#endif

/*
 * Add support for G_LOG_LEVEL_INFO through tracker_info().
 */
#ifdef G_HAVE_ISO_VARARGS
#define tracker_info(...)         g_log (G_LOG_DOMAIN,	\
                                         G_LOG_LEVEL_INFO,	\
                                         __VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
#define tracker_info(format...)   g_log (G_LOG_DOMAIN,	\
                                         G_LOG_LEVEL_INFO,	\
                                         format)
#else   /* no varargs macros */
static void
tracker_info (const gchar *format,
              ...)
{
	va_list args;
	va_start (args, format);
	g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, format, args);
	va_end (args);
}
#endif  /* !__GNUC__ */

gboolean tracker_log_init          (gint             verbosity,
                                    gchar          **used_filename);
void     tracker_log_shutdown      (void);

G_END_DECLS

#endif /* __LIBTRACKER_COMMON_LOG_H__ */
