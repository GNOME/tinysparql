/*
 * Copyright (C) 2008-2009, Nokia
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

namespace Tracker {
	[CCode (cheader_filename = "libtracker-common/tracker-class.h")]
	public class Class : GLib.Object {
		public string name { get; set; }
		public string uri { get; set; }
	}

	[CCode (cheader_filename = "libtracker-common/tracker-namespace.h")]
	public class Namespace : GLib.Object {
		public string prefix { get; set; }
		public string uri { get; set; }
	}

	[CCode (cheader_filename = "libtracker-common/tracker-property.h")]
	public class Property : GLib.Object {
		public string name { get; set; }
		public string uri { get; set; }
		public PropertyType data_type { get; set; }
		public Class domain { get; set; }
		public Class range { get; set; }
		public bool multiple_values { get; set; }
	}

	[CCode (cheader_filename = "libtracker-common/tracker-property.h")]
	public enum PropertyType {
		STRING,
		BOOLEAN,
		INTEGER,
		DOUBLE,
		DATE,
		DATETIME,
		BLOB,
		STRUCT,
		RESOURCE,
		FULLTEXT
	}

	[CCode (cheader_filename = "libtracker-common/tracker-ontology.h")]
	namespace Ontology {
		public weak Class get_class_by_uri (string class_uri);
		public weak Property get_property_by_uri (string property_uri);
		[CCode (array_length = false, array_null_terminated = true)]
		public weak Namespace[] get_namespaces ();
		[CCode (array_length = false, array_null_terminated = true)]
		public weak Class[] get_classes ();
		[CCode (array_length = false, array_null_terminated = true)]
		public weak Property[] get_properties ();
	}

	[CCode (cheader_filename = "libtracker-common/tracker-type-utils.h")]
	public int string_to_date (string date_string);
}

