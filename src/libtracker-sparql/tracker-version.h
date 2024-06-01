/*
 * Copyright (C) 2006, Jamie McCracken <jamiemcc@gnome.org>
 * Copyright (C) 2008, Nokia <ivan.frade@nokia.com>
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

#pragma once

#include <glib.h>

G_BEGIN_DECLS

#if !defined (__LIBTRACKER_SPARQL_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "only <tinysparql.h> must be included directly."
#endif

#ifndef _TRACKER_EXTERN
#define _TRACKER_EXTERN __attribute__((visibility("default"))) extern
#endif

#define _TRACKER_UNAVAILABLE(maj, min) G_UNAVAILABLE(maj, min) _TRACKER_EXTERN
#define _TRACKER_DEPRECATED G_DEPRECATED _TRACKER_EXTERN
#define _TRACKER_DEPRECATED_FOR(f) G_DEPRECATED_FOR(f) _TRACKER_EXTERN

#define TRACKER_VERSION_3_0 G_ENCODE_VERSION (3, 0)
#define TRACKER_VERSION_3_1 G_ENCODE_VERSION (3, 1)
#define TRACKER_VERSION_3_2 G_ENCODE_VERSION (3, 2)
#define TRACKER_VERSION_3_3 G_ENCODE_VERSION (3, 3)
#define TRACKER_VERSION_3_4 G_ENCODE_VERSION (3, 4)
#define TRACKER_VERSION_3_5 G_ENCODE_VERSION (3, 5)
#define TRACKER_VERSION_3_6 G_ENCODE_VERSION (3, 6)
#define TRACKER_VERSION_3_7 G_ENCODE_VERSION (3, 7)
#define TRACKER_VERSION_3_8 G_ENCODE_VERSION (3, 8)
#define TRACKER_VERSION_CUR G_ENCODE_VERSION (TRACKER_MAJOR_VERSION, TRACKER_MINOR_VERSION)

#ifndef TRACKER_VERSION_MIN_REQUIRED
#define TRACKER_VERSION_MIN_REQUIRED TRACKER_VERSION_CUR
#endif

#ifndef TRACKER_VERSION_MAX_ALLOWED
#define TRACKER_VERSION_MAX_ALLOWED TRACKER_VERSION_CUR
#endif

#if TRACKER_VERSION_MIN_REQUIRED > TRACKER_VERSION_MAX_ALLOWED
#error "TRACKER_VERSION_MAX_ALLOWED must be >= TRACKER_VERSION_MIN_REQUIRED"
#endif

#define TRACKER_AVAILABLE_IN_ALL _TRACKER_EXTERN

/* 3.1 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_1
#define TRACKER_DEPRECATED_IN_3_1 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_1_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_1 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_1_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_1
#define TRACKER_AVAILABLE_IN_3_1 _TRACKER_UNAVAILABLE(3, 1)
#else
#define TRACKER_AVAILABLE_IN_3_1 _TRACKER_EXTERN
#endif

/* 3.2 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_2
#define TRACKER_DEPRECATED_IN_3_2 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_2_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_2 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_2_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_2
#define TRACKER_AVAILABLE_IN_3_2 _TRACKER_UNAVAILABLE(3, 2)
#else
#define TRACKER_AVAILABLE_IN_3_2 _TRACKER_EXTERN
#endif

/* 3.3 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_3
#define TRACKER_DEPRECATED_IN_3_3 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_3_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_3 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_3_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_3
#define TRACKER_AVAILABLE_IN_3_3 _TRACKER_UNAVAILABLE(3, 3)
#else
#define TRACKER_AVAILABLE_IN_3_3 _TRACKER_EXTERN
#endif

/* 3.4 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_4
#define TRACKER_DEPRECATED_IN_3_4 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_4_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_4 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_4_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_4
#define TRACKER_AVAILABLE_IN_3_4 _TRACKER_UNAVAILABLE(3, 4)
#else
#define TRACKER_AVAILABLE_IN_3_4 _TRACKER_EXTERN
#endif

/* 3.5 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_5
#define TRACKER_DEPRECATED_IN_3_5 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_5_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_5 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_5_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_5
#define TRACKER_AVAILABLE_IN_3_5 _TRACKER_UNAVAILABLE(3, 5)
#else
#define TRACKER_AVAILABLE_IN_3_5 _TRACKER_EXTERN
#endif

/* 3.6 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_6
#define TRACKER_DEPRECATED_IN_3_6 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_6_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_6 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_6_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_6
#define TRACKER_AVAILABLE_IN_3_6 _TRACKER_UNAVAILABLE(3, 6)
#else
#define TRACKER_AVAILABLE_IN_3_6 _TRACKER_EXTERN
#endif

/* 3.7 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_7
#define TRACKER_DEPRECATED_IN_3_7 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_7_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_7 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_7_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_7
#define TRACKER_AVAILABLE_IN_3_7 _TRACKER_UNAVAILABLE(3, 7)
#else
#define TRACKER_AVAILABLE_IN_3_7 _TRACKER_EXTERN
#endif

/* 3.8 */
#if TRACKER_VERSION_MIN_REQUIRED >= TRACKER_VERSION_3_8
#define TRACKER_DEPRECATED_IN_3_8 _TRACKER_DEPRECATED
#define TRACKER_DEPRECATED_IN_3_8_FOR(f) _TRACKER_DEPRECATED_FOR(f)
#else
#define TRACKER_DEPRECATED_IN_3_8 _TRACKER_EXTERN
#define TRACKER_DEPRECATED_IN_3_8_FOR(f) _TRACKER_EXTERN
#endif

#if TRACKER_VERSION_MAX_ALLOWED < TRACKER_VERSION_3_8
#define TRACKER_AVAILABLE_IN_3_8 _TRACKER_UNAVAILABLE(3, 8)
#else
#define TRACKER_AVAILABLE_IN_3_8 _TRACKER_EXTERN
#endif

/**
 * tracker_major_version:
 *
 * The major version of the Tracker library.
 *
 * An integer variable exported from the library linked against at application run time.
 */
GLIB_VAR const guint tracker_major_version;

/**
 * tracker_minor_version:
 *
 * The minor version of the Tracker library.
 *
 * An integer variable exported from the library linked against at application run time.
 */
GLIB_VAR const guint tracker_minor_version;

/**
 * tracker_micro_version:
 *
 * The micro version of the Tracker library.
 *
 * An integer variable exported from the library linked against at application run time.
 */
GLIB_VAR const guint tracker_micro_version;

/**
 * tracker_interface_age:
 *
 * The interface age of the Tracker library. Defines how far back the API has last been extended.
 *
 * An integer variable exported from the library linked against at application run time.
 */
GLIB_VAR const guint tracker_interface_age;

/**
 * tracker_binary_age:
 *
 * The binary age of the Tracker library. Defines how far back backwards compatibility reaches.
 *
 * An integer variable exported from the library linked against at application run time.
 */
GLIB_VAR const guint tracker_binary_age;

/**
 * TRACKER_CHECK_VERSION:
 * @major: the required major version.
 * @minor: the required minor version.
 * @micro: the required micro version.
 *
 * This macro essentially does the same thing as
 * tracker_check_version() but as a pre-processor operation rather
 * than a run-time operation. It will evaluate true or false based the
 * version passed in and the version available.
 *
 * <example>
 * <title>Simple version check example</title>
 * An example of how to make sure you have the version of Tracker
 * installed to run your code.
 * <programlisting>
 * if (!TRACKER_CHECK_VERSION (0, 10, 7)) {
 *         g_error ("Tracker version 0.10.7 or above is needed");
 * }
 * </programlisting>
 * </example>
 **/
#define TRACKER_CHECK_VERSION(major,minor,micro)    \
    (TRACKER_MAJOR_VERSION > (major) || \
     (TRACKER_MAJOR_VERSION == (major) && TRACKER_MINOR_VERSION > (minor)) || \
     (TRACKER_MAJOR_VERSION == (major) && TRACKER_MINOR_VERSION == (minor) && \
      TRACKER_MICRO_VERSION >= (micro)))

TRACKER_AVAILABLE_IN_ALL
const gchar * tracker_check_version (guint required_major,
                                     guint required_minor,
                                     guint required_micro);

G_END_DECLS
