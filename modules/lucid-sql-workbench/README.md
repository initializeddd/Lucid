# SQL Workbench (draft)

The host has a SQLite snapshot query backend, but this draft is not yet wired to
the editor. It needs a project-local database picker, a query input, a results and
query-plan view, and explicit reporting of errors and truncated results.

The backend executes one read-only SQLite query against an in-memory copy of a
standalone database of at most 64 MiB. It does not modify the original database,
load SQLite extensions, attach databases, or access an external SQL server. It
rejects WAL-format databases because reading the database file alone would miss
changes in the WAL; use an exported standalone snapshot. Queries and output are
bounded, and cancelled or incomplete results must be shown as such. The backend
does not yet support editing data, transactions, parameters, or live databases.
Its limits and snapshot requirement are implementation restrictions, not a claim
that SQLite execution of untrusted files is a security sandbox.
