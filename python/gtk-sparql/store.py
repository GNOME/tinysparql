# GtkSparql - Gtk UI to try SparQL queries against tracker.
# Copyright (C) 2009, Ivan Frade <ivan.frade@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
import sqlite3
import os

DEFAULT_EXAMPLE = "SELECT ?u \nWHERE { ?u a nie:InformationElement. }\n\n"
EMPTY_QUERY = "SELECT \nWHERE { \n\n}"

class QueriesStore ():

    def __init__ (self):
        create = False
        self.db_dir, self.db_path =  self.get_db_dir_file ()

        if (not os.path.exists (self.db_dir)):
            os.makedirs (self.db_dir)
            create = True

        if (create or not os.path.exists (self.db_path)):
            conn = sqlite3.connect (self.db_path)
            c = conn.cursor ()
            c.execute ("CREATE TABLE saved_queries ( Name Text not null, Query Text );")
            c.execute ("INSERT INTO saved_queries VALUES ('', '%s')" % (EMPTY_QUERY))
            c.execute ("INSERT INTO saved_queries VALUES ('Example', '%s')" % (DEFAULT_EXAMPLE))
            conn.commit ()
            c.close ()

    def get_db_dir_file (self):
        return (os.path.join (os.getenv ("HOME"), ".local", "share", "tracker-query"),
                os.path.join (os.getenv ("HOME"), ".local", "share", "tracker-query", "queries.db"))

    def save_query (self, name, value):
        conn = sqlite3.connect (self.db_path)
        c = conn.cursor ()
        c.execute ("INSERT INTO saved_queries VALUES ('%s', '%s');" % (name, value))
        conn.commit ()
        c.close ()

    def delete_query (self, name):
        conn = sqlite3.connect (self.db_path)
        c = conn.cursor ()
        c.execute ("DELETE FROM saved_queries WHERE Name='%s';" % (name))
        conn.commit ()
        c.close ()


    def get_all_queries (self):
        conn = sqlite3.connect (self.db_path)
        c = conn.cursor ()
        c.execute ("SELECT * FROM saved_queries;")
        results =  c.fetchall ()
        c.close ()
        return results
