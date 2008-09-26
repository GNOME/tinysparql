/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with main.c; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */

/*
 * This file contains special tools for debugging, used in xesam-glib
 */

#ifndef _XESAM_G_DEBUG_PRIVATE_H_
#define _XESAM_G_DEBUG_PRIVATE_H_

#include "config.h"

G_BEGIN_DECLS

#ifndef XESAM_G_LOG_DOMAIN
#define XESAM_G_LOG_DOMAIN    "XesamGLib"
#endif	/* XESAM_G_LOG_DOMAIN */

/*
 * Make xesam_g_debug a noop if ENABLE_DEBUG is not defined
 */
#ifdef ENABLE_DEBUG

#   ifdef G_HAVE_ISO_VARARGS
#	   define xesam_g_debug(...)	g_log (XESAM_G_LOG_DOMAIN, \
										   G_LOG_LEVEL_DEBUG,  \
										   __VA_ARGS__)
#	   define xesam_g_debug_object(object, ...)    xesam_g_debug_object_real (object, __VA_ARGS__)

#   elif defined(G_HAVE_GNUC_VARARGS)
#	   define xesam_g_debug(format...)  g_log (XESAM_G_LOG_DOMAIN,	 \
											   G_LOG_LEVEL_DEBUG,	\
											   format)
#	   define xesam_g_debug_object(object, format...)    xesam_g_debug_object_real (object, format)
#   else   /* no varargs macros */
static void
xesam_g_debug (const gchar *format,
			   ...)
{
	va_list args;
	va_start (args, format);
	g_logv (XESAM_G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
	va_end (args);
}

static void
xesam_g_debug_object (GObject *obj,
					  const gchar *format,
					  ...)
{
	va_list args;
	va_start (args, format);
	xesam_g_debug_object_va (obj, format, args);
	va_end (args);
}
#   endif  /* !__GNUC__ */

#else /* NO DEBUGGING OUTPUT */

#   ifdef G_HAVE_ISO_VARARGS
#	   define xesam_g_debug(...)	G_STMT_START{ (void)0; }G_STMT_END
#	   define xesam_g_debug_object(object, ...)    G_STMT_START{ (void)0; }G_STMT_END
#   elif defined(G_HAVE_GNUC_VARARGS)
#	   define xesam_g_debug(format...)     G_STMT_START{ (void)0; }G_STMT_END
#	   define xesam_g_debug_object(object, format...)     G_STMT_START{ (void)0; }G_STMT_END
#   else   /* no varargs macros */

static void xesam_g_debug (const gchar *format, ...) { ; }
static void xesam_g_debug_object (GObject *obj, const gchar *format, ...) { ; }

#   endif /* !__GNUC__ */

#endif /* ENABLE_DEBUG */

G_END_DECLS

#endif /* _XESAM_G_DEBUG_PRIVATE_H_ */
