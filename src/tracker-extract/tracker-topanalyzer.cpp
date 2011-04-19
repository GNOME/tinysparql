/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2008, Nokia
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
 *
  * Authors: Philip Van Hoof <philip@codeminded.be>
 */

#include <glib.h>
#include <glib/gstdio.h>

#include <strigi/indexwriter.h>
#include <strigi/analysisresult.h>
#include <strigi/analyzerconfiguration.h>
#include <strigi/fileinputstream.h>

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <map>
#include <sstream>
#include <algorithm>

#include <libtracker-common/tracker-type-utils.h>
#include <libtracker-common/tracker-os-dependant.h>
#include <libtracker-common/tracker-sparql-builder.h>
#include <libtracker-common/tracker-ontologies.h>

#include <libtracker-extract/tracker-utils.h>

#define NIE_PREFIX TRACKER_NIE_PREFIX

#include "tracker-main.h"
#include "tracker-topanalyzer.h"

using namespace std;
using namespace Strigi;

static GStaticPrivate private_key = G_STATIC_PRIVATE_INIT;

namespace Tracker {
	class TripleCollector : public Strigi::IndexWriter
	{
	public:
		TripleCollector        ();
		~TripleCollector       ();

		void commit            ();
		void deleteEntries     (const std::vector<std::string>& entries);
		void deleteAllEntries  ();
		void initWriterData    (const Strigi::FieldRegister&);
		void releaseWriterData (const Strigi::FieldRegister&);
		void startAnalysis     (const AnalysisResult*);
		void addText           (const AnalysisResult*,
		                        const char* text,
		                        int32_t length);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        const std::string& value);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        const unsigned char* data,
		                        uint32_t size);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        int32_t value);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        uint32_t value);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        double value);
		void addTriplet        (const std::string& subject,
		                        const std::string& predicate,
		                        const std::string& object);
		void addValue          (const AnalysisResult*,
		                        const RegisteredField* field,
		                        const std::string& name,
		                        const std::string& value);
		void finishAnalysis    (const AnalysisResult*);
		void setParams         (const gchar *uri_,
		                        TrackerSparqlBuilder *metadata_);

		gchar                  *content_type;

	private:
		const gchar *predicateMapping (const RegisteredField *field);
		const gchar *predicateMapping (const std::string &key);
		gboolean     predicateNeeded  (const gchar *predicate);

		const gchar                   *uri;
		TrackerSparqlBuilder          *metadata;
	};

	Tracker::TripleCollector::TripleCollector ()
	{
		content_type = NULL;
	}

	void Tracker::TripleCollector::setParams (const gchar *uri_, TrackerSparqlBuilder *metadata_)
	{
		uri = uri_;
		metadata = metadata_;
		g_free (content_type);
		content_type = NULL;
	}

	Tracker::TripleCollector::~TripleCollector ()
	{
		g_free (content_type);
	}

	void Tracker::TripleCollector::commit () { }
	void Tracker::TripleCollector::deleteEntries (const std::vector<std::string>& entries ) { }
	void Tracker::TripleCollector::deleteAllEntries () { }
	void Tracker::TripleCollector::initWriterData (const Strigi::FieldRegister&) { }
	void Tracker::TripleCollector::releaseWriterData (const Strigi::FieldRegister&) { }
	void Tracker::TripleCollector::startAnalysis (const AnalysisResult* idx) { }

	void Tracker::TripleCollector::addText (const AnalysisResult* idx,
	                                        const char* text,
	                                        int32_t length)
	{

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, NIE_PREFIX "plainTextContent");
		tracker_sparql_builder_object_unvalidated (metadata, text);
	}

	const gchar* Tracker::TripleCollector::predicateMapping (const std::string &key)
	{
		/* const gchar *original; */
		/* gchar *str, *p; */

		/* original = key.c_str(); */

		/* p = strrchr (original, '/'); */
		/* if (G_UNLIKELY (!p)) { */
		/* 	return g_strdup (original); */
		/* } */

		/* if (G_UNLIKELY (!strchr (p, '#'))) { */
		/* 	return g_strdup (original); */
		/* } */

		/* str = g_strdup (p + 1); */
		/* p = strchr (str, '#'); */
		/* *p = ':'; */

		return key.c_str();
	}

	const gchar* Tracker::TripleCollector::predicateMapping (const RegisteredField *field)
	{
		/* const gchar *original; */
		/* gchar *str, *p; */

		/* original = field->key().c_str(); */

		/* p = strrchr (original, '/'); */
		/* if (G_UNLIKELY (!p)) { */
		/* 	return g_strdup (original); */
		/* } */

		/* if (G_UNLIKELY (!strchr (p, '#'))) { */
		/* 	return g_strdup (original); */
		/* } */

		/* str = g_strdup (p + 1); */
		/* p = strchr (str, '#'); */
		/* *p = ':'; */

		return field->key().c_str();
	}

	gboolean Tracker::TripleCollector::predicateNeeded (const gchar *predicate)
	{
		if (!predicate) {
			return FALSE;
		}

		/* We already cover these in the miner-fs */
		if (strstr (predicate, "nfo#FileDataObject") ||
		    strstr (predicate, "nfo#belongsToContainer") ||
		    strstr (predicate, "nfo#fileName") ||
		    strstr (predicate, "nfo#fileSize") ||
		    strstr (predicate, "nfo#fileLastModified") ||
		    strstr (predicate, "nfo#fileLastAccessed") ||
		    strstr (predicate, "nie#InformationElement") ||
		    strstr (predicate, "nie#isStoredAs") ||
		    strstr (predicate, "nie#mimeType") ||
		    strstr (predicate, "nie#dataSource")) {
			return FALSE;
		}

		return TRUE;
	}

	/* The methods below basically just convert the C++ world to the C world
	 * of TrackerSparqlBuilder. Nothing magical about it. */

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         const std::string& value)
	{
		const gchar *predicate = predicateMapping (field);

		if (field->key() == FieldRegister::mimetypeFieldName && idx->depth() == 0) {
			g_free (content_type);
			content_type = g_strdup (value.c_str());
		}

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_unvalidated (metadata, value.c_str());
	}

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         const unsigned char* data,
	                                         uint32_t size )
	{
		const gchar *predicate = predicateMapping (field);

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_unvalidated (metadata, (const gchar*) data);
	}

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         int32_t value)
	{
		const gchar *predicate = predicateMapping (field);

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_int64 (metadata, value);

	}

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         uint32_t value )
	{
		const gchar *predicate = predicateMapping (field);

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_int64 (metadata, value);
	}

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         double value )
	{
		const gchar *predicate = predicateMapping (field);

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_double (metadata, value);
	}

	void Tracker::TripleCollector::addTriplet (const std::string& subject,
	                                           const std::string& predicate,
	                                           const std::string& object )
	{
		const gchar *predicate_str = predicateMapping (predicate);

		if (!predicateNeeded (predicate_str)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, subject.c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate_str);
		tracker_sparql_builder_object_unvalidated (metadata, object.c_str());
	}

	void Tracker::TripleCollector::addValue (const AnalysisResult* idx,
	                                         const RegisteredField* field,
	                                         const std::string& name,
	                                         const std::string& value )
	{
		const gchar *predicate = predicateMapping (field);

		if (field->key() == FieldRegister::mimetypeFieldName && idx->depth() == 0) {
			g_free (content_type);
			content_type = g_strdup (value.c_str());
		}

		if (!predicateNeeded (predicate)) {
			return;
		}

		tracker_sparql_builder_subject_iri (metadata, idx->path().c_str());
		tracker_sparql_builder_predicate_iri (metadata, predicate);
		tracker_sparql_builder_object_unvalidated (metadata, value.c_str());
	}

	void Tracker::TripleCollector::finishAnalysis (const AnalysisResult*) { }
}

typedef struct {
	Strigi::AnalyzerConfiguration *mconfig;
	Strigi::StreamAnalyzer *streamindexer;
	Tracker::TripleCollector *m_writer;
} TrackerTopanalyzerPrivate;

static void
private_free (gpointer data)
{
	TrackerTopanalyzerPrivate *priv = (TrackerTopanalyzerPrivate*) data;

	delete priv->mconfig;
	delete priv->streamindexer;
	delete priv->m_writer;

	g_free (priv);
}

void
tracker_topanalyzer_init (void)
{
	TrackerTopanalyzerPrivate *priv;

	/* For added granularity of what analyzer should be elected for which
	 * filetype or file, you can inherit a Strigi::AnalyzerConfiguration
	 * and have some tuning this way. */

	FieldRegister::FieldRegister ();

	priv = g_new0 (TrackerTopanalyzerPrivate, 1);

	priv->mconfig = new Strigi::AnalyzerConfiguration ();
	priv->streamindexer = new Strigi::StreamAnalyzer (*priv->mconfig);
	priv->m_writer = new Tracker::TripleCollector ();

	priv->streamindexer->setIndexWriter (*priv->m_writer);

	g_static_private_set (&private_key,
	                      priv,
	                      private_free);
}

void
tracker_topanalyzer_shutdown (void)
{
	g_static_private_set (&private_key, NULL, NULL);
}

void
tracker_topanalyzer_extract (const gchar           *uri,
                             TrackerSparqlBuilder  *metadata,
                             gchar                **content_type)
{
	TrackerTopanalyzerPrivate *priv;
	gchar *filename;

	priv = (TrackerTopanalyzerPrivate*) g_static_private_get (&private_key);
	g_return_if_fail (priv != NULL);

	/* We need the filename from the URI because we'll use stat() and because
	 * in this experiment I used FileInputStream. But any kind of stream could
	 * work with StreamAnalyzer's analyzers. */

	filename = g_filename_from_uri (uri, NULL, NULL);

	if (filename) {
		struct stat s;

		/* We use our own strategy as writer. Our writer writes to the @metadata
		 * array. I decided to call it a collector because that's what its
		 * implementation does (collecting triples) */

		priv->m_writer->setParams (uri, metadata);
		stat (filename, &s);

		/* The first parameter that we pass here will influence what
		 * idx->path() will be above. StreamAnalyzer only ever appends
		 * path chunks to this initial stringvalue. So if we pass
		 * our://URI then idx->path will end up being:
		 *
		 * our://URI
		 * our://URI/child
		 * our://URI/child/child.
		 *
		 * For example the URI of a tar.gz will go like this:
		 *
		 * file:///path/to/my.tar.gz
		 * file:///path/to/my.tar.gz/dir_in_tar/file1.txt
		 * file:///path/to/my.tar.gz/dir_in_tar/file2.txt
		 *
		 * The URI passed here doesn't mean the stream passed later must
		 * not really resemble the URI. Usually it will of course.
		 */

		AnalysisResult analysisresult (uri, s.st_mtime, *priv->m_writer,
		                               *priv->streamindexer);

		/* If we want a remote stream, then we implement a Stream in C++
		 * for it and use that instead of FileInputStream. We could for
		 * example make a C++ wrapper for GInputStream and enjoy using
		 * GIO and GNIO here that way. */

		FileInputStream resource (filename);

		if (resource.status() == Ok) {
			analysisresult.index (&resource);

			if (content_type && priv->m_writer->content_type) {
				*content_type = g_strdup (priv->m_writer->content_type);
			}
		}

		g_free (filename);
	}
}

