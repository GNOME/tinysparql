/*
 * Copyright (C) 2010, Adrien Bustany (abustany@gnome.org)
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

#include "config.h"
#include <libtracker-miner/tracker-password-provider.h>

#define SERVICE_NAME "TestService"

int main (int argc, char **argv)
{
	gchar *username = NULL;
	gchar *password = NULL;
	GError *error = NULL;

	g_type_init ();
	g_set_application_name ("PasswordBackendTest");

	TrackerPasswordProvider *provider = tracker_password_provider_get ();

	tracker_password_provider_store_password (provider,
	                                          SERVICE_NAME,
	                                          "This is the test service",
	                                          "testUser",
	                                          "testPass",
	                                          &error);

	if (error) {
		g_critical ("tracker_password_provider_store: %s", error->message);
		g_error_free (error);

		return 1;
	}

	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   &username,
	                                                   &error);

	if (error) {
		g_critical ("tracker_password_provider_get: %s", error->message);
		g_error_free (error);
		g_free (username);
		g_free (password);

		return 1;
	} else {
		g_message ("Username: %s , Password:%s", username, password);
	}

	g_free (username);
	g_free (password);

	// Also test without getting the username
	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   NULL,
	                                                   &error);

	if (error) {
		g_critical ("tracker_password_provider_get: %s", error->message);
		g_error_free (error);
		g_free (password);

		return 1;
	} else {
		g_message ("Password:%s", password);
	}

	g_free (password);
	g_object_unref (provider);
	return 0;
}
