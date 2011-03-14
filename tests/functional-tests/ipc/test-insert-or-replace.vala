using GLib;
using Tracker;
using Tracker.Sparql;

const string insert_query_replace = "
DELETE {
  ?r nao:hasProperty ?property .
} WHERE {
    ?r a nco:PhoneNumber;
	nco:phoneNumber \"02141730585%d\";
	nao:hasProperty ?property .
}

DELETE {
  ?r a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber,
	      nco:VideoTelephoneNumber, nco:MessagingNumber,
	      nco:VoicePhoneNumber, nco:CellPhoneNumber,
	      nco:FaxNumber, nco:ModemNumber .
} WHERE {
  ?r a nco:PhoneNumber;
	nco:phoneNumber \"02141730585%d\" .
}

INSERT {
  _:tel a nco:PhoneNumber ;
	nco:phoneNumber \"02141730585%d\" .
} WHERE {
  OPTIONAL {
    ?r a nco:PhoneNumber;
	nco:phoneNumber \"02141730585%d\" .
  }
  FILTER(!bound(?r)) .
}

DELETE { <mailto:rhome0@example.com%d> ?predicate ?object . }
WHERE {
  <mailto:rhome0@example.com%d> ?predicate ?object .
  FILTER(?predicate IN (nco:emailAddress)) .
}

INSERT {
    <mailto:rhome0@example.com%d> a nco:EmailAddress ;
		nco:emailAddress \"rhome0@example.com%d\" .
}

DELETE { <contact:r:%d> nco:hasAffiliation ?e }
WHERE { <contact:r:%d> nco:hasAffiliation ?e }

INSERT OR REPLACE { 
    _:af1 a nco:Affiliation ;
		rdfs:label \"Work\" ;
		nco:hasEmailAddress <mailto:rhome0@example.com%d> .

    _:af2 a nco:Affiliation ;
		rdfs:label \"Other\" ;
		nco:hasPhoneNumber ?tel .

	<contact:r:%d> a nco:PersonContact ;
		nco:nameHonorificPrefix \"Mrs.\" ;
		nco:nameGiven \"First %d\" ;
		nco:nameFamily \"Last %d\" ;
		nco:hasAffiliation _:af1 ;
		nco:hasAffiliation _:af2 ;
		nie:generator \"addressbook\" ;
		nie:contentLastModified \"2011-03-14T13:47:25\" ;
		nie:contentCreated \"2011-03-14T13:47:25\" ;
		nco:contactUID \"c1f1b12d-bc75-4d45-9a1f-b1efe934409f\" .
} WHERE {
  ?tel nco:phoneNumber \"02141730585%d\"
}
";

const string insert_query_orig = "
DELETE { <mailto:home0@example.com%d> ?predicate ?object . }
WHERE {
  <mailto:home0@example.com%d> ?predicate ?object .
  FILTER(?predicate IN (nco:emailAddress)) .
}

DELETE { ?resource nao:hasProperty ?property . }
WHERE {
  ?resource nao:hasProperty ?property ; nco:phoneNumber ?key .
  FILTER(?key IN (\"2141730585%d\")) .
}

DELETE {
  ?resource a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber,
	      nco:VideoTelephoneNumber, nco:MessagingNumber,
	      nco:VoicePhoneNumber, nco:CellPhoneNumber,
	      nco:FaxNumber, nco:ModemNumber .
} WHERE {
  ?resource a nco:PhoneNumber ; nco:phoneNumber ?key .
  FILTER(?key IN (\"2141730585%d\")) .
}

INSERT {
  _:_ a nco:PhoneNumber ; nco:phoneNumber \"2141730585%d\" .
} WHERE {
  OPTIONAL {
    ?resource a nco:PhoneNumber ; nco:phoneNumber \"2141730585%d\" .
  }
  FILTER(!bound(?resource)) .
}

DELETE { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {
    <contact:o:%d> ?predicate ?object .
} } WHERE { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {
    <contact:o:%d> ?predicate ?object .
    FILTER(?predicate NOT IN (nco:contactLocalUID,nco:contactUID,rdf:type)) .
} }

INSERT { GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9> {
    <mailto:home0@example.com%d> a nco:EmailAddress ;
		nco:emailAddress \"home0@example.com%d\" .

    _:_Affiliation_Work1 a nco:Affiliation ;
		rdfs:label \"Work\" ;
		nco:hasEmailAddress <mailto:home0@example.com%d> .

    ?_PhoneNumber_Resource3 a nco:VoicePhoneNumber, nco:PhoneNumber ;
		nco:phoneNumber \"2141730585%d\" .

    _:_Affiliation_Other2 a nco:Affiliation ;
		rdfs:label \"Other\" ;
		nco:hasPhoneNumber ?_PhoneNumber_Resource3 .

    <contact:o:%d> a nco:PersonContact ;
		nco:nameHonorificPrefix \"Mrs.\" ;
		nco:nameGiven \"First %d\" ;
		nco:nameFamily \"Last %d\" ;
		nco:hasAffiliation _:_Affiliation_Work1 ;
		nco:hasAffiliation _:_Affiliation_Other2 ;
		nie:generator \"addressbook\" ;
		nie:contentLastModified \"2011-03-14T13:47:25\" ;
		nie:contentCreated \"2011-03-14T13:47:25\" ;
		nco:contactUID \"c1f1b12d-bc75-4d45-9a1f-b1efe934409f\" .
} } WHERE {
  ?_PhoneNumber_Resource3 nco:phoneNumber \"2141730585%d\"
}
";

int
main( string[] args )
{
	uint i, y = 100;
	Timer t1 = new Timer ();
	Timer t2 = new Timer ();
	Connection c;
	c = Connection.get ();

	t1.start ();
	for (i = 0; i < y; i++) {
		c.update (insert_query_replace.printf (i,i,i,i,i,i,i,i,i,i,i,i,i,i,i));
	}
	t1.stop ();

	print ("REPLACE: %f\n", t1.elapsed());


	t2.start ();
	for (i = 0; i < y; i++) {
		c.update (insert_query_orig.printf (i,i,i,i,i,i,i,i,i,i,i,i,i,i,i,i));
	}
	t2.stop ();

	print ("ORIGINAL: %f\n", t2.elapsed());

	return 0;
}
