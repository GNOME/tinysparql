The project tree is structured the following way:

- `cli` contains the command line utilities installed as `tinysparql`.
- `common` contains the private static library common between the rest of
  components.
- `http` contains the private static library to implement web servers and
  clients. This library is used by libtinysparql for SPARQL HTTP endpoints
  and the CLI tools for the web-based IDE.
- `libtinysparql` contains the SPARQL library
- `ontologies` contains the various data ontologies offered/used by
  this project.
- `portal` contains the XDG portal to allow sandboxed access to SPARQL D-Bus
  endpoints.
- `web-ide` contains the server-side code of the web-based IDE.
