# Dependency Risk Review (work in progress)

The module reads npm package-lock v1/v2/v3, Go go.sum, Cargo.lock v3/v4 and Pipfile.lock.
For npm it identifies exact versions from canonical npm registry tarball URLs;
direct dependencies are not inferred for v1. With explicit user consent it can send
package names and versions to OSV's batch API. It processes per-package pages
and replaces results only after the whole request succeeds; cancellation,
network failures, malformed responses and response limits leave the previous
report intact. File paths and source code are not included in requests.

Pipfile.lock support covers exactly pinned canonical public-PyPI versions from
the default and develop groups, with the canonical TLS-verified index declared.
Markers, installed packages, directness and artifact hashes are not verified;
other sources and unsupported versions are skipped.

OSV returns IDs, not a determination of exploitability, and its advisory data
may be incomplete. Advisory details can be fetched after separate confirmation.
Aliases, other registries, links, other ecosystems, and unmapped dependencies
are reported as gaps. Transitive chains and package publication remain unfinished.
