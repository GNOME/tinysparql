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

#include <stdlib.h>

#include <libtracker-miner/tracker-password-provider.h>

#define SERVICE_NAME  "TestService"
#define TEST_USERNAME "test-user"
#define TEST_PASSWORD "s3cr3t"

int 
main (int argc, char **argv)
{
	TrackerPasswordProvider *provider;
	gchar *username = NULL;
	gchar *password = NULL;
	GError *error = NULL;

	g_type_init ();
	g_set_application_name ("PasswordBackendTest");

	g_print ("Testing TrackerPasswordProvider...\n");

	provider = tracker_password_provider_get ();

	g_print ("Storing password '%s' for user '%s'\n",
	         TEST_PASSWORD,
	         TEST_USERNAME);

	tracker_password_provider_store_password (provider,
	                                          SERVICE_NAME,
	                                          "This is the test service",
	                                          TEST_USERNAME,
	                                          TEST_PASSWORD,
	                                          &error);

	if (error) {
		g_printerr ("Calliung tracker_password_provider_store_password() failed, %s", 
		            error->message);
		g_error_free (error);
		g_object_unref (provider);

		return EXIT_FAILURE;
	}

	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   &username,
	                                                   &error);

	if (error) {
		g_printerr ("Calling tracker_password_provider_get_password() failed, %s", 
		            error->message);
		g_error_free (error);
		g_free (username);
		g_free (password);
		g_object_unref (provider);

		return EXIT_FAILURE;
	}

	g_print ("Found password is '%s' for username '%s'\n", 
	         password,
	         username);

	g_free (username);
	g_free (password);

	/* Also test without getting the username */
	password = tracker_password_provider_get_password (provider,
	                                                   SERVICE_NAME,
	                                                   NULL,
	                                                   &error);

	if (error) {
		g_printerr ("Calling tracker_password_provider_get_password() failed, %s", 
		            error->message);
		g_error_free (error);
		g_free (password);
		g_object_unref (provider);

		return EXIT_SUCCESS;
	}

	g_print ("Found password is '%s' for NULL username\n", password);

	g_free (password);
	g_object_unref (provider);

	g_print ("Done\n");

	return EXIT_SUCCESS;
}
