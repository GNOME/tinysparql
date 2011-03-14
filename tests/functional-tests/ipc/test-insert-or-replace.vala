using GLib;
using Tracker;
using Tracker.Sparql;

const string insert_query_orig = "
DELETE
{
  ?subject nco:hasPostalAddress ?object .
}
WHERE
{
  <contact_b:%d> nco:hasAffiliation ?subject .
  ?subject nco:hasPostalAddress ?object .
}

DELETE
{
  ?subject nco:hasPostalAddress ?object .
}
WHERE
{
  <contact_b:%d> nco:hasAffiliation [ nco:org ?subject ] .
  ?subject nco:hasPostalAddress ?object .
}

DELETE
{
  ?resource a rdfs:Resource .
}
WHERE
{
  <contact_b:%d> ?predicate ?resource .

  FILTER(?predicate IN (nao:hasProperty, nco:hasPostalAddress, ncal:anniversary,
                        ncal:birthday, nco:hasLocation, nco:hasAffiliation)) .
}

DELETE
{
  <mailto:andre@andrews.com> ?predicate ?object .
}
WHERE
{
  <mailto:andre@andrews.com> ?predicate ?object .
  FILTER(?predicate IN (nco:emailAddress)) .
}

DELETE
{
  <mailto:andre.andrews@meego.com> ?predicate ?object .
}
WHERE
{
  <mailto:andre.andrews@meego.com> ?predicate ?object .
  FILTER(?predicate IN (nco:emailAddress)) .
}

DELETE
{
  <tel:+4917212345%d> ?predicate ?object .
}
WHERE
{
  <tel:+4917212345%d> ?predicate ?object .
  FILTER(?predicate IN (nao:hasProperty, nco:phoneNumber)) .
}

DELETE
{
  <tel:+4916134567%d> ?predicate ?object .
}
WHERE
{
  <tel:+4916134567%d> ?predicate ?object .
  FILTER(?predicate IN (nao:hasProperty, nco:phoneNumber)) .
}

DELETE
{
  ?resource a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber, nco:VideoTelephoneNumber,
              nco:MessagingNumber, nco:VoicePhoneNumber, nco:CellPhoneNumber, nco:FaxNumber,
              nco:ModemNumber .
}
WHERE
{
  ?resource a nco:PhoneNumber .
  FILTER(?resource IN (<tel:+4917212345%d>, <tel:+4916134567%d>)) .
}

DELETE
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    <contact_b:%d> ?predicate ?object .
  }
}
WHERE
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    <contact_b:%d> ?predicate ?object .
    FILTER(?predicate NOT IN (nco:contactLocalUID,nco:contactUID,rdf:type)) .
  }
}

INSERT
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    <contact_b:%d> rdf:type nco:PersonContact .
    <contact_b:%d> nco:contactLocalUID \"%d\" .
    <contact_b:%d> nco:gender nco:gender-male .

    <contact_b:%d> nco:photo <avatar:photo:sleeping-bonobo> .
    <contact_b:%d> nco:video <avatar:video:crazy-banana> .

    <contact_b:%d> nco:nameHonorificPrefix \"Sir\" .
    <contact_b:%d> nco:nameGiven \"Andre b %d\" .
    <contact_b:%d> nco:nameFamily \"Andrews b %d\" .

    _:_Affiliation_Work000000001 rdf:type nco:Affiliation .
    _:_Affiliation_Work000000001 rdfs:label \"Work\" .
    <contact_b:%d> nco:hasAffiliation _:_Affiliation_Work000000001 .

    _:_Organization_Resource000000004 rdf:type nco:OrganizationContact .
    _:_Affiliation_Work000000001 nco:org _:_Organization_Resource000000004 .

    _:_Organization_Resource000000004 nco:logo <avatar:photo:boring-cube> .

    _:Affiliation_Home1 rdf:type nco:Affiliation .
    _:Affiliation_Home1 rdfs:label \"Home\" .
    <contact_b:%d> nco:hasAffiliation _:Affiliation_Home1 .

    <mailto:andre@andrews.com>  rdf:type nco:EmailAddress .
    _:Affiliation_Home1 nco:hasEmailAddress <mailto:andre@andrews.com> .
    <mailto:andre@andrews.com> nco:emailAddress \"andre@andrews.com\" .

    _:Affiliation_Other1 rdf:type nco:Affiliation .
    _:Affiliation_Other1 rdfs:label \"Other\" .
    <contact_b:%d> nco:hasAffiliation _:Affiliation_Other1 .

    <mailto:andre.andrews@meego.com>  rdf:type nco:EmailAddress .
    _:Affiliation_Other1 nco:hasEmailAddress <mailto:andre.andrews@meego.com> .
    <mailto:andre.andrews@meego.com> nco:emailAddress \"andre.andrews@meego.com\" .

    <tel:+4917212345%d> rdf:type nco:VoicePhoneNumber .
    _:Affiliation_Home1 nco:hasPhoneNumber <tel:+4917212345%d> .
    <tel:+4917212345%d> nco:phoneNumber \"+4917212345%d\" .

    <tel:+4916134567%d>  rdf:type nco:CellPhoneNumber .
    _:Affiliation_Home1 nco:hasPhoneNumber <tel:+4916134567%d> .
    <tel:+4916134567%d> nco:phoneNumber \"+4916134567%d\" .

    _:Affiliation_Home1 nco:url \"http://andrews.com/\" .

    _:Address_Resource4 rdf:type nco:DomesticDeliveryAddress .
    _:Affiliation_Home1 nco:hasPostalAddress _:Address_Resource4 .
    _:Address_Resource4 nco:country \"Germany\" .
    _:Address_Resource4 nco:locality \"Berlin\" .
    _:Address_Resource4 nco:streetAddress \"Alexanderplatz 1\" .

    _:Address_Resource5 rdf:type nco:ParcelDeliveryAddress .
    _:_Affiliation_Work000000001 nco:hasPostalAddress _:Address_Resource5 .
    _:Address_Resource5 nco:country \"Germany\" .
    _:Address_Resource5 nco:locality \"Berlin\" .
    _:Address_Resource5 nco:streetAddress \"Friedrichstrasse 105\" .

    _:Anniversary_Resource3 rdf:type ncal:Event .
    <contact_b:%d> ncal:anniversary _:Anniversary_Resource3 .
    _:Anniversary_Resource3 ncal:uid \"11223344\" .
    _:Anniversary_Resource3 ncal:dtstart \"2007-05-05T00:00:00\" .
    _:Anniversary_Resource3 ncal:description \"Hochzeit\" .
    _:Anniversary_Resource3 ncal:categories \"Wedding\" .

    <contact_b:%d> nie:generator \"addressbook\" .
    <contact_b:%d> nco:hobby \"fishing\" .

    ?_Tag_Resource000000010 rdf:type nao:Tag .
    <contact_b:%d> nao:hasTag ?_Tag_Resource000000010 .
    ?_Tag_Resource000000010 nao:prefLabel \"Knorke\" .

    <contact_b:%d> nie:contentLastModified \"2010-05-04T09:30:00Z\" .
    <contact_b:%d> nie:contentCreated \"2010-04-22T01:00:00Z\" .
  }
}
";

const string prepare_query_replace = "
INSERT {
	<avatar:photo:sleeping-bonobo> a nfo:FileDataObject ;
			nie:url \"file:///home/user/.contacts/avatars/sleeping-bonobo.png\" .
	<avatar:video:crazy-banana> a nfo:FileDataObject ;
			nie:url \"file:///home/user/.contacts/avatars/crazy-banana.ogv\" .
	<avatar:photo:boring-cube> a nfo:FileDataObject ;
			nie:url \"file:///home/user/.contacts/avatars/boring-cube.png\" .
}";

const string insert_query_replace = "
DELETE
{
  ?subject nco:hasPostalAddress ?object .
}
WHERE
{
  <contact:%d> nco:hasAffiliation ?subject .
  ?subject nco:hasPostalAddress ?object .
}

DELETE
{
  ?subject nco:hasPostalAddress ?object .
}
WHERE
{
  <contact:%d> nco:hasAffiliation [ nco:org ?subject ] .
  ?subject nco:hasPostalAddress ?object .
}

DELETE
{
  ?resource a rdfs:Resource .
}
WHERE
{
  <contact:%d> ?predicate ?resource .

  FILTER(?predicate IN (nao:hasProperty, nco:hasPostalAddress, ncal:anniversary,
                        ncal:birthday, nco:hasLocation, nco:hasAffiliation)) .
}

DELETE
{
  <tel:+4917212345%d> nao:hasProperty ?object .
}
WHERE
{
  <tel:+4917212345%d> nao:hasProperty ?object .
}

DELETE
{
  <tel:+4916134567%d> nao:hasProperty ?object .
}
WHERE
{
  <tel:+4916134567%d> nao:hasProperty ?object .
}

DELETE
{
  ?resource a nco:CarPhoneNumber, nco:BbsNumber, nco:PagerNumber, nco:VideoTelephoneNumber,
              nco:MessagingNumber, nco:VoicePhoneNumber, nco:CellPhoneNumber, nco:FaxNumber,
              nco:ModemNumber .
}
WHERE
{
  ?resource a nco:PhoneNumber .
  FILTER(?resource IN (<tel:+4917212345%d>, <tel:+4916134567%d>)) .
}

DELETE
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    <contact:%d> ?predicate ?object .
  }
}
WHERE
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    <contact:%d> ?predicate ?object .
    FILTER(?predicate NOT IN (nco:contactLocalUID,nco:contactUID,rdf:type)) .
  }
}

INSERT OR REPLACE
{
  GRAPH <urn:uuid:08070f5c-a334-4d19-a8b0-12a3071bfab9>
  {
    ?_Tag a nao:Tag ; nao:prefLabel \"Knorke\" .

    <mailto:andre@andrews.com> a nco:EmailAddress ;
			nco:emailAddress \"andre@andrews.com\" .

    <mailto:andre.andrews@meego.com> a nco:EmailAddress ;
			nco:emailAddress \"andre.andrews@meego.com\" .

    <tel:+4917212345%d> a nco:VoicePhoneNumber ; nco:phoneNumber \"+4917212345%d\" .

    <tel:+4916134567%d> a nco:CellPhoneNumber ;
			nco:phoneNumber \"+4916134567%d\" .

    _:Anniversary a ncal:Event ;
			ncal:uid \"11223344\" ;
			ncal:dtstart \"2007-05-05T00:00:00\" ;
			ncal:description \"Hochzeit\" ;
			ncal:categories \"Wedding\" .

    _:Address_Resource1 a nco:DomesticDeliveryAddress ;
			nco:country \"Germany\" ;
			nco:locality \"Berlin\" ;
			nco:streetAddress \"Alexanderplatz 1\" .

    _:Address_Resource2 a nco:ParcelDeliveryAddress ;
			nco:country \"Germany\" ;
			nco:locality \"Berlin\" ;
			nco:streetAddress \"Friedrichstrasse 105\" .

    _:Organization_Resource a nco:OrganizationContact ;
			nco:logo <avatar:photo:boring-cube> .

    _:Affiliation_Work a nco:Affiliation ; rdfs:label \"Work\" ;
			nco:org _:_Organization_Resource ;
			nco:hasPostalAddress _:Address_Resource2 ;
			nco:hasPhoneNumber <tel:+4916134567%d> .

    _:Affiliation_Home a nco:Affiliation ; rdfs:label \"Home\" ;
			nco:hasEmailAddress <mailto:andre@andrews.com> ;
			nco:hasPhoneNumber <tel:+4917212345%d> ;
			nco:url \"http://andrews.com/\" ;
			nco:hasPostalAddress _:Address_Resource1 .

    _:Affiliation_Other a nco:Affiliation ; rdfs:label \"Other\" ;
			nco:hasEmailAddress <mailto:andre.andrews@meego.com> .

    <contact:%d> a nco:PersonContact ;
			nco:contactLocalUID \"%d\" ;
			nco:gender nco:gender-male ;
			nco:photo <avatar:photo:sleeping-bonobo> ;
			nco:video <avatar:video:crazy-banana> ;
			nco:nameHonorificPrefix \"Sir\" ;
			nco:nameGiven \"Andre%d\" ;
			nco:nameFamily \"Andrews%d\" ;
			nco:hasAffiliation _:Affiliation_Work ;
			nco:hasAffiliation _:Affiliation_Home ;
			nco:hasAffiliation _:Affiliation_Other ;
			ncal:anniversary _:Anniversary ;
			nie:generator \"addressbook\" ;
			nco:hobby \"fishing\" ;
			nao:hasTag ?_Tag ;
			nie:contentLastModified \"2010-05-04T09:30:00Z\" ;
			nie:contentCreated \"2010-04-22T01:00:00Z\" .
  }
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

	c.update (prepare_query_replace);

	t1.start ();
	for (i = 0; i < y; i++) {
		c.update (insert_query_replace.printf (i, i, i, i, i, i, i, i, i, i));
	}
	t1.stop ();

	print ("%f\n", t1.elapsed());


	t2.start ();
	for (i = 0; i < y; i++) {
		c.update (insert_query_orig.printf (i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i, i));
	}
	t2.stop ();

	print ("%f\n", t2.elapsed());

	return 0;
}
