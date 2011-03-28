#!/usr/bin/env python
import gi
from gi.repository import Tracker, GObject

def results_ready_cb (obj, result, user_data):
    cursor = obj.query_finish (result)

    # This can also be done asynchronously
    while (cursor.next (None)):
        print cursor.get_string (0)

    user_data.quit ()


if __name__ == "__main__":
    loop = GObject.MainLoop ()

    # The connection can be requested asynchronously
    conn = Tracker.SparqlConnection.get (None)
    conn.query_async ("SELECT ?u WHERE { ?u a nie:InformationElement. }",
                      None,
                      results_ready_cb,
                      loop)

    loop.run ()
