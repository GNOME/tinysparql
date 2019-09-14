# Copyright (C) 2010, Nokia <ivan.frade@nokia.com>
# Copyright (C) 2019, Sam Thursfield <sam@afuera.me.uk>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
"""
Replicate the behaviour of the miner inserting information in the store.
"""
import unittest as ut
from storetest import CommonTrackerStoreTest as CommonTrackerStoreTest


class TestMinerInsertBehaviour (CommonTrackerStoreTest):
    """
    Mimic the behaviour of the miner, removing the previous information of the resource
    and inserting a new one.
    """

    def test_miner_unique_insertion(self):
        """
        We actually can't test tracker-miner-fs, so we mimick its behavior in this test
        1. Insert one resource
        2. Update it like tracker-miner-fs does
        3. Check there isn't any duplicate
        4. Clean up
        """

        resource = 'graph://test/resource/1'

        insert_sparql = """
        INSERT INTO <graph://test/resource/1> {
           _:resource a nie:DataObject ;
                      nie:url "%s" .
        }
        """ % resource

        select_sparql = """
        SELECT ?u { ?u nie:url "%s" }
        """ % resource

        delete_sparql = """
        DELETE { GRAPH <graph://test/resource/1> { ?r a rdfs:Resource } } WHERE { GRAPH <graph://test/resource/1> { ?r a rdfs:Resource } }
        """

        ''' First insertion '''
        self.tracker.update(insert_sparql)

        results = self.tracker.query(select_sparql)
        self.assertEqual(len(results), 1)

        ''' Second insertion / update '''
        self.tracker.update(delete_sparql + ' ' + insert_sparql)

        results = self.tracker.query(select_sparql)
        self.assertEqual(len(results), 1)

        ''' Clean up '''
        self.tracker.update(delete_sparql)

        results = self.tracker.query(select_sparql)
        self.assertEqual(len(results), 0)


if __name__ == '__main__':
    ut.main()
