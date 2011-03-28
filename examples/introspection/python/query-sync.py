#!/usr/bin/env python
import gi
from gi.repository import Tracker

conn = Tracker.SparqlConnection.get (None)
cursor = conn.query ("SELECT ?u WHERE { ?u a nie:InformationElement. }", None)

while (cursor.next (None)):
    print cursor.get_string (0)
