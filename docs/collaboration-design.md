# Collaboration implementation status

Multiplayer is **not available in the editor yet**. The code in
`src/workspace/collaboration` is a foundation for a host-authoritative project
session, not a supported feature or a server to expose to other people.

The host model keeps file state and cursors per participant, rebases disjoint
concurrent edits, and rejects overlapping or stale edits without changing the
host document. The replica model retains local text when it cannot safely merge.
A bounded host listener and client transport exchange versioned, framed
messages using a random bearer token. The loopback test runs multiple clients.
Protocol version 3 requires the canonical file path and the per-opening context
from the snapshot on edit, cursor, and save requests. Delayed operations cannot
affect a different file, including an older opening of the same path.
Authenticated clients can request a current file list with a `files` message;
the response echoes the request ID and provides a generation, next offset, and
truncation flag. Later pages include that generation; a changed project list
requires browsing to restart. File scanning runs on a background worker, with one scan at a time
to bound resource use; editor integration and large-project testing remain.

The transport does **not** encrypt traffic. A future user-facing release must
require a trusted encrypted overlay such as Tailscale, protect share links as
secrets, and review authentication and denial-of-service controls. The current
listener is restricted to IPv4 loopback and 100.64.0.0/10 addresses; that
address check alone does not establish that a network is encrypted. Display
names are unverified labels.

Before exposing hosting or joining, Lucid still needs editor integration,
remote project browsing, reconnect and conflict-resolution UI, durable recovery
for unsaved host and client edits, a safe host shutdown flow, and Windows
transport. The separate self-hosted service and full multi-device testing also
remain unfinished. Do not use the internal listener to share real work yet.
