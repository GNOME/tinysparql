/*
** 2006 Oct 10
**
** The author disclaims copyright to this source code.	In place of
** a legal notice, here is a blessing:
**
**    May you do good and not evil.
**    May you find forgiveness for yourself and forgive others.
**    May you share freely, never taking more than you give.
**
******************************************************************************
**
** This header file is used by programs that want to link against the
** FTS3 library.
*/

#ifndef __TRACKER_FTS_H__
#define __TRACKER_FTS_H__

#include <sqlite3.h>
#include <glib.h>

G_BEGIN_DECLS

typedef const gchar *(*TrackerFtsMapFunc) (gint id);

int tracker_fts_init (sqlite3 *db, int create);
void tracker_fts_shutdown (void);
void tracker_fts_set_map_function (TrackerFtsMapFunc map_func);
int tracker_fts_update_init (int id);
int tracker_fts_update_text (int id, int column_id, const char *text, gboolean limit_word_length);
void tracker_fts_update_commit (void);
void tracker_fts_update_rollback (void);

G_END_DECLS

#endif /* __TRACKER_FTS_H__ */

