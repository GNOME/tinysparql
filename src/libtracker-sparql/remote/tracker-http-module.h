/*
 * Copyright (C) 2022, Red Hat Inc.
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
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 */

#ifndef TRACKER_HTTP_MODULE_H
#define TRACKER_HTTP_MODULE_H

#include "tracker-http.h"

#define TRACKER_TYPE_HTTP_SERVER_SOUP (tracker_http_server_soup_get_type ())
G_DECLARE_FINAL_TYPE (TrackerHttpServerSoup,
                      tracker_http_server_soup,
                      TRACKER, HTTP_SERVER_SOUP,
                      TrackerHttpServer)

#define TRACKER_TYPE_HTTP_CLIENT_SOUP (tracker_http_client_soup_get_type ())
G_DECLARE_FINAL_TYPE (TrackerHttpClientSoup,
                      tracker_http_client_soup,
                      TRACKER, HTTP_CLIENT_SOUP,
                      TrackerHttpClient)

#endif /* TRACKER_HTTP_MODULE_H */
