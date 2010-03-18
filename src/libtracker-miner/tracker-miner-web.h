/*
 * Copyright (C) 2009, Adrien Bustany (abustany@gnome.org)
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

#ifndef __LIBTRACKER_MINER_WEB_H__
#define __LIBTRACKER_MINER_WEB_H__

#include <libtracker-miner/tracker-miner.h>

#define TRACKER_TYPE_MINER_WEB         (tracker_miner_web_get_type())
#define TRACKER_MINER_WEB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_WEB, TrackerMinerWeb))
#define TRACKER_MINER_WEB_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_WEB, TrackerMinerWebClass))
#define TRACKER_IS_MINER_WEB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_WEB))
#define TRACKER_IS_MINER_WEB_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_WEB))
#define TRACKER_MINER_WEB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_WEB, TrackerMinerWebClass))

#define TRACKER_MINER_WEB_ASSOCIATION_TYPE_DB (tracker_miner_web_association_get_type ())

G_BEGIN_DECLS

typedef struct TrackerMinerWeb TrackerMinerWeb;
typedef struct TrackerMinerWebPrivate TrackerMinerWebPrivate;

/**
 * The name of the DBus interface exposed by the web miners
 **/
#define TRACKER_MINER_WEB_DBUS_INTERFACE "org.freedesktop.Tracker1.MinerWeb"

/**
 * TrackerMinerWebError:
 * @TRACKER_MINER_WEB_ERROR_WRONG_CREDENTIALS: The stored credentials are refused by the remote service
 * @TRACKER_MINER_WEB_ERROR_TOKEN_EXPIRED    : The service says the stored token has expired
 * @TRACKER_MINER_WEB_ERROR_NO_CREDENTIALS   : There are currenty no credentials stored for this service
 * @TRACKER_MINER_WEB_ERROR_KEYRING          : Error while contacting the credentials storage
 * @TRACKER_MINER_WEB_ERROR_SERVICE          : Error while contacting the remote service
 * @TRACKER_MINER_WEB_ERROR_TRACKER          : Error while contacting Tracker
 *
 * Describes the different errors that can occur while operating with the remote service.
 **/
typedef enum {
	TRACKER_MINER_WEB_ERROR_WRONG_CREDENTIALS,
	TRACKER_MINER_WEB_ERROR_TOKEN_EXPIRED,
	TRACKER_MINER_WEB_ERROR_NO_CREDENTIALS,
	TRACKER_MINER_WEB_ERROR_KEYRING,
	TRACKER_MINER_WEB_ERROR_SERVICE,
	TRACKER_MINER_WEB_ERROR_TRACKER
} TrackerMinerWebError;

#define TRACKER_MINER_WEB_ERROR        tracker_miner_web_error_quark ()
#define TRACKER_MINER_WEB_ERROR_DOMAIN "TrackerMinerWeb"

/**
 * TrackerMinerWebAssociationStatus:
 * @TRACKER_MINER_WEB_UNASSOCIATED : The miner is currently unassociated with the remote service
 * @TRACKER_MINER_WEB_ASSOCIATED   : The miner is currently associated with the remote service
 *
 * Describes if the miner can currently communicate (import data) with the web service.
 **/
typedef enum {
	TRACKER_MINER_WEB_UNASSOCIATED,
	TRACKER_MINER_WEB_ASSOCIATED
} TrackerMinerWebAssociationType;

struct TrackerMinerWeb {
	TrackerMiner            parent_instance;
};

/**
 * TrackerMinerWebClass
 * @parent_class        : parent object class
 * @authenticate        : called when the miner is told to authenticate against the remote service
 * @get_association_data: called when one requests the miner's association data.
 *                        In the case of a user/pass based authentication, this should return
 *                        an empty hash table.
 *                        In the case of a token based authentication, the following keys must
 *                        be defined in the returned hash table:
 *                        - url: the url to point the user too
 *                        Additionally, the miner can define the following keys :
 *                        - post_message: a message to display after the user completes the
 *                                        association process
 *                        - post_url: a url to point the user after he completes the association
 *
 *                        If both post_message and post_url are defined, the message will be
 *                        displayed before the url is opened.
 * @associate           : called when the miner is told to associate with the web service.
 *                        In the case of a user/pass based authentication, the following keys must be defined
 *                        - username: the provided username
 *                        - password: the provided password
 *                        In the case of a token based authentication, the hash table can be ignored
 *                        since it will be empty.
 * @dissociate          : called when the miner is told to forget any user credentials it has stored
 **/
typedef struct {
	TrackerMinerClass parent_class;

	/* vmethods */
	void        (* authenticate)         (TrackerMinerWeb   *miner,
	                                      GError           **error);
	GHashTable* (* get_association_data) (TrackerMinerWeb   *miner,
	                                      GError           **error);
	void        (* associate)            (TrackerMinerWeb   *miner,
	                                      const GHashTable  *association_data,
	                                      GError           **error);
	void        (* dissociate)           (TrackerMinerWeb   *miner,
	                                      GError           **error);
} TrackerMinerWebClass;

GType       tracker_miner_web_get_type             (void) G_GNUC_CONST;
GType       tracker_miner_web_association_get_type (void) G_GNUC_CONST;
GQuark      tracker_miner_web_error_quark          (void);
void        tracker_miner_web_authenticate         (TrackerMinerWeb  *miner,
                                                    GError          **error);
GHashTable *tracker_miner_web_get_association_data (TrackerMinerWeb  *miner,
                                                    GError          **error);
void        tracker_miner_web_associate            (TrackerMinerWeb  *miner,
                                                    GHashTable       *association_data,
                                                    GError          **error);
void        tracker_miner_web_dissociate           (TrackerMinerWeb  *miner,
                                                    GError          **error);

G_END_DECLS

#endif /* __LIBTRACKER_MINER_WEB_H__ */
