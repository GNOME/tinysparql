/*
 * Copyright (C) 2009, Adrien Bustany <abustany@gnome.org>
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

#if !defined (__LIBTRACKER_MINER_H_INSIDE__) && !defined (TRACKER_COMPILATION)
#error "Only <libtracker-miner/tracker-miner.h> can be included directly."
#endif

#include <libtracker-miner/tracker-miner-object.h>

#define TRACKER_TYPE_MINER_WEB         (tracker_miner_web_get_type())
#define TRACKER_MINER_WEB(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TRACKER_TYPE_MINER_WEB, TrackerMinerWeb))
#define TRACKER_MINER_WEB_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c),    TRACKER_TYPE_MINER_WEB, TrackerMinerWebClass))
#define TRACKER_IS_MINER_WEB(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TRACKER_TYPE_MINER_WEB))
#define TRACKER_IS_MINER_WEB_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c),    TRACKER_TYPE_MINER_WEB))
#define TRACKER_MINER_WEB_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o),  TRACKER_TYPE_MINER_WEB, TrackerMinerWebClass))

G_BEGIN_DECLS

typedef struct TrackerMinerWeb TrackerMinerWeb;
typedef struct TrackerMinerWebPrivate TrackerMinerWebPrivate;

/**
 * TRACKER_MINER_WEB_DBUS_INTERFACE:
 * The name of the DBus interface exposed by the web miners
 **/
#define TRACKER_MINER_WEB_DBUS_INTERFACE "org.freedesktop.Tracker1.MinerWeb"

/**
 * TrackerMinerWebError:
 * @TRACKER_MINER_WEB_ERROR_WRONG_CREDENTIALS: The credentials were
 * refused by the remote service
 * @TRACKER_MINER_WEB_ERROR_TOKEN_EXPIRED: The remote service
 * token has expired
 * @TRACKER_MINER_WEB_ERROR_NO_CREDENTIALS: There are currenty no
 * credentials stored for this service
 * @TRACKER_MINER_WEB_ERROR_KEYRING: The credential storage failed to
 * retrieve the relevant information. See <classname>TrackerPasswordProvider</classname>
 * @TRACKER_MINER_WEB_ERROR_SERVICE: Could not contact the remote service
 * @TRACKER_MINER_WEB_ERROR_TRACKER: Could not contact the Tracker service
 *
 * The following errors are possible during any of the performed
 * actions with a remote service.
 *
 * Since: 0.8
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

struct TrackerMinerWeb {
	TrackerMiner            parent_instance;
};

/**
 * TrackerMinerWebClass
 * @parent_class: parent object class
 * @authenticate: called when a miner must authenticate against a
 * remote service
 * @get_association_data: called when one requests the miner's
 * associated data
 * @associate: called when the miner is asked to associate a remote
 * service
 * @dissociate: called when the miner is asked to revoke an
 * association with a remote service
 *
 * For the @authenticate function, a username/password can be used and
 * this should return an empty %GHashTable. If the authentication is
 * based on a token, the following keys <emphasis>must</emphasis> be
 * returned in the %GHashTable:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <emphasis>url</emphasis>: the url to point the user to.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * Additionally, the miner <emphasis>may</emphasis> define the
 * following keys:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <emphasis>post_message</emphasis>: a message to display after
 *       the user completes the association process.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis>post_url</emphasis>: a url to point the user to
 *       after they have completed the association.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * <note>
 *   <para>
 *      If both <emphasis>post_message</emphasis> and
 *      <emphasis>post_url</emphasis> are defined, the message will be
 *      displayed before the url is opened.
 *   </para>
 * </note>
 *
 * For the @associate function, in the case of a username/password
 * based authentication, the following keys must be defined:
 *
 * <itemizedlist>
 *   <listitem>
 *     <para>
 *       <emphasis>username</emphasis>: the username.
 *     </para>
 *   </listitem>
 *   <listitem>
 *     <para>
 *       <emphasis>password</emphasis>: the password.
 *     </para>
 *   </listitem>
 * </itemizedlist>
 *
 * In the case of a token based authentication, the %GHashTable can be
 * ignored since it will be empty.
 *
 * For the @dissociate function, miners <emphasis>must</emphasis>
 * forget any user credentials stored.
 *
 * Since: 0.8.
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
