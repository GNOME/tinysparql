#!/usr/bin/env python
import gi
from gi.repository import Tracker, GObject


def results_ready_cb (obj, result, user_data):
    cursor = obj.query_finish (result)

    # This can also be done asynchronously
    while (cursor.next (None)):
        print cursor.get_string (0)

    user_data.quit ()

def connection_ready_cb (object, result, user_data):
    assert user_data
    conn = Tracker.SparqlConnection.get_finish (result)

    conn.query_async ("SELECT ?u WHERE { ?u a nie:InformationElement. }",
                      None,
                      results_ready_cb,
                      user_data)


if __name__ == "__main__":
    loop = GObject.MainLoop ()

    Tracker.SparqlConnection.get_async (None, connection_ready_cb, loop)

    loop.run ()
