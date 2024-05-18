Title: Security considerations
Slug: security-considerations

The SPARQL 1.1 specifications have a number of informative `Security
considerations` sections. This is an informative document describing how
those may or may not apply to the implementation of TinySPARQL.

Note that most of these considerations derive from situations where
a RDF triple store is exposed through a public endpoint, while TinySPARQL
does not do that by default. Users should be careful about creating
endpoints. For D-Bus endpoints, access through the portal is encouraged.

# SPARQL specifications

## Queries

(From [https://www.w3.org/TR/2013/REC-sparql11-query-20130321/#security](https://www.w3.org/TR/2013/REC-sparql11-query-20130321/#security))
```
SPARQL queries using FROM, FROM NAMED, or GRAPH may cause the specified URI to
be dereferenced. This may cause additional use of network, disk or CPU resources
along with associated secondary issues such as denial of service. The security
issues of Uniform Resource Identifier (URI): Generic Syntax [RFC3986] Section 7
should be considered. In addition, the contents of file: URIs can in some cases
be accessed, processed and returned as results, providing unintended access to
local resources.

SPARQL requests may cause additional requests to be issued from the SPARQL
endpoint, such as FROM NAMED. The endpoint is potentially within an
organisations firewall or DMZ, and so such queries may be a source of
indirection attacks.
```

Graph URIs are virtual in TinySPARQL and do not cause any access outside of
database resources. The only SPARQL syntax capable of dereferencing or accessing
external resources are the `SERVICE <uri>` and `LOAD <rdf-file>` features.


(From [https://www.w3.org/TR/2013/REC-sparql11-query-20130321/#security](https://www.w3.org/TR/2013/REC-sparql11-query-20130321/#security))
```
The SPARQL language permits extensions, which will have their own security
implications.
```

TinySPARQL extensions have no special security considerations, other than
being code that runs on silicon.

## Federated queries

(From [https://www.w3.org/TR/2013/REC-sparql11-federated-query-20130321/#security](https://www.w3.org/TR/2013/REC-sparql11-federated-query-20130321/#security))
```
SPARQL queries using SERVICE imply that a URI will be dereferenced, and that the
result will be incorporated into a working data set.
```

(From [https://www.w3.org/TR/sparql11-protocol/#policy-security](https://www.w3.org/TR/sparql11-protocol/#policy-security))
```
Since a SPARQL protocol service may make HTTP requests of other origin servers
on behalf of its clients, it may be used as a vector of attacks against other
sites or services. Thus, SPARQL protocol services may effectively act as proxies
for third-party clients. Such services may place restrictions on the resources
that they retrieve or on the rate at which external resources can be retrieved.
SPARQL protocol services may log client requests in such a way as to facilitate
tracing them with regard to third-party origin servers or services.

SPARQL protocol services may choose to detect these and other costly, or
otherwise unsafe, queries, impose time or memory limits on queries, or impose
other restrictions to reduce the service's (and other service's) vulnerability
to denial-of-service attacks. They also may refuse to process such query
requests.
```

TinySPARQL offers 2 types of endpoint that are susceptible to this vector:

- D-Bus endpoints accessed outside a sandbox.
- HTTP endpoints

Particularly, requests on a D-Bus endpoint happening through the portal from a
sandboxed process have all SERVICE access restricted.

TinySPARQL developers encourage that all access to endpoints created on D-Bus
happen through the portal, and that all HTTP endpoints validate the provenance
of the requests through the [signal@EndpointHttp::block_remote_address]
signal to limit access to resources.

(From [https://www.w3.org/TR/sparql11-protocol/#policy-security](https://www.w3.org/TR/sparql11-protocol/#policy-security))
```
There are at least two possible sources of denial-of-service attacks against
SPARQL protocol services. First, under-constrained queries can result in very
large numbers of results, which may require large expenditures of computing
resources to process, assemble, or return. Another possible source are queries
containing very complex — either because of resource size, the number of
resources to be retrieved, or a combination of size and number — RDF Dataset
descriptions, which the service may be unable to assemble without significant
expenditure of resources, including bandwidth, CPU, or secondary storage. In
some cases such expenditures may effectively constitute a denial-of-service
attack. A SPARQL protocol service may place restrictions on the resources that
it retrieves or on the rate at which external resources are retrieved. There
may be other sources of denial-of-service attacks against SPARQL query
processing services.
```

TinySPARQL does not perform any time or frequency rate limits to queries. HTTP
endpoints may perform the latter through the
[signal@EndpointHttp::block_remote_address] signal.

## Updates

(From [https://www.w3.org/TR/2013/REC-sparql11-update-20130321/#security](https://www.w3.org/TR/2013/REC-sparql11-update-20130321/#security))
```
Write access to data makes it inherently vulnerable to malicious access.
Standard access and authentication techniques should be used in any networked
environment. In particular, HTTPS should be used, especially when implementing
the SPARQL HTTP-based protocols. (i.e., encryption with challenge/response based
password presentation, encrypted session tokens, etc). Some of the weak points
addressed by HTTPS are: authentication, active session integrity between client
and server, preventing replays, preventing continuation of defunct sessions.
```

(From [https://www.w3.org/TR/sparql11-protocol/#policy-security](https://www.w3.org/TR/sparql11-protocol/#policy-security))
```
SPARQL protocol services may remove, insert, and change underlying data via the
update operation. To protect against malicious or destructive updates,
implementations may choose not to implement the update operation. Alternatively,
implementations may choose to use HTTP authentication mechanisms or other
implementation-defined mechanisms to prevent unauthorized invocations of the
update operation.
```

TinySPARQL HTTP endpoints do not implement any update mechanisms. D-Bus endpoints
accessed through the portal from inside a sandbox are likewise read-only.


(From [https://www.w3.org/TR/2013/REC-sparql11-update-20130321/#security](https://www.w3.org/TR/2013/REC-sparql11-update-20130321/#security))
```
Systems that provide both read-only and writable interfaces can be subject to
injection attacks in the read-only interface. In particular, a SPARQL endpoint
with a Query service should be careful of injection attacks aimed at interacting
with an Update service on the same SPARQL endpoint. Like any client code,
interaction between the query service and the update service should ensure
correct escaping of strings provided by the user.

While SPARQL Update and SPARQL Query are separate languages, some
implementations may choose to offer both at the same SPARQL endpoint. In this
case, it is important to consider that an Update operation may be obscured to
masquerade as a query. For instance, a string of unicode escapes in a PREFIX
clause could be used to hide an Update Operation. Therefore, simple syntactic
tests are inadequate to determine if a string describes a query or an update.
```

Following the SPARQL 1.1 spec, TinySPARQL implements updates and queries as two
different languages with different parser entry points, this separation happens
all the way to the public API. As an additional layer of security, readonly
queries happen on readonly database connections. It is essentially not possible
to perform any data change from the query APIs.

## IRIs

IRIs are a cornerstone of RDF data, since individual RDF resources are typically
named through them. Quoting [https://www.w3.org/TR/sparql11-protocol/#policy-security](https://www.w3.org/TR/sparql11-protocol/#policy-security):

```
Different IRIs may have the same appearance. Characters in different scripts
may look similar (a Cyrillic "о" may appear similar to a Latin "o"). A
character followed by combining characters may have the same visual
representation as another character (LATIN SMALL LETTER E followed by
COMBINING ACUTE ACCENT has the same visual representation as LATIN SMALL
LETTER E WITH ACUTE). Users of SPARQL must take care to construct queries
with IRIs that match the IRIs in the data. Further information about matching
of similar characters can be found in Unicode Security Considerations
[UNISEC] and Internationalized Resource Identifiers (IRIs) [RFC3987]
Section 8.
```

The situations where this might be a source of confusion or mischief, or even
be possible depends on how those IRIs are created, used, displayed or
inserted.

# API user considerations

Users of the TinySPARQL API and query language are encouraged to make some
considerations and take some precautions:

 * Do not expose any endpoints that does not need exposing.
 * For local D-Bus endpoints, consider using a graph partitioning scheme that
   makes it easy to policy the access to the data when accessed through the
   portal.
 * Avoid the possibility of injection attacks. Use [class@SparqlStatement]
   and avoid string-based approaches to build SPARQL queries from user input.
 * Consider that IRIs describing RDF resources are susceptible to homograph
   attacks as described above. Developers are not exempt of validating
   external input that might end up stored as-is.


# Feature grid

This is a quick reference of the features offered by the different types of
endpoint.

| Endpoint       | Query | Update | Graph Constraints | Service Constraints |
|----------------|-------|--------|-------------------|---------------------|
| D-Bus (portal) | ✓     | ✗      | ✓                 | ✓                   |
| D-Bus          | ✓     | ✓      | ✗                 | ✗                   |
| HTTP           | ✓     | ✗      | ✗                 | ✗                   |
