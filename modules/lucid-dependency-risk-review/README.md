# Dependency Risk Review (work in progress)

The backend reads npm package-lock v2/v3 and identifies exact versions from
canonical npm registry tarball URLs. With explicit user consent it can send
package names and versions to OSV's batch API. It processes per-package pages
and replaces results only after the whole request succeeds; cancellation,
network failures, malformed responses and response limits leave the previous
report intact. File paths and source code are not included in requests.

OSV returns IDs, not a determination of exploitability, and its advisory data
may be incomplete. Aliases, other registries, links, other ecosystems, and
unmapped dependencies are reported as gaps. Advisory details, attribution of
transitive chains, the editor workflow, and package publication are unfinished.
