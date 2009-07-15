/*
 * Copyright (C) 2009, Nokia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

namespace Tracker {
	[CCode (cheader_filename = "libtracker-data/tracker-data-update.h")]
	public errordomain DataError {
		UNKNOWN_CLASS,
		UNKNOWN_PROPERTY,
		INVALID_TYPE
	}

	[CCode (cheader_filename = "libtracker-data/tracker-data-query.h,libtracker-data/tracker-data-update.h")]
	namespace Data {
		public int query_resource_id (string uri);
		public void begin_transaction ();
		public void commit_transaction ();
		public void delete_statement (string subject, string predicate, string object);
		public void insert_statement (string subject, string predicate, string object) throws DataError;
		public void insert_statement_with_uri (string subject, string predicate, string object) throws DataError;
		public void insert_statement_with_string (string subject, string predicate, string object) throws DataError;
		public void delete_resource_description (string uri);
	}
}

