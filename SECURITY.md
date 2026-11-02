# Security policy

## Supported versions

Lucid is pre-1.0. Security fixes are applied to the latest `main` branch and the most recent
release.

| Version | Supported |
| --- | :---: |
| Latest release | ✅ |
| `main` | ✅ |
| Older releases | ❌ |

## Reporting a vulnerability

Please do **not** open a public issue for a security problem. Instead:

1. Use GitHub's private vulnerability reporting: **Security → Report a vulnerability** in this
   repository.
2. Include a description, affected version or commit, reproduction steps, and the impact you
   expect.
3. Allow time for a fix before disclosing the issue publicly.

You will receive an acknowledgement as soon as possible. Confirmed issues are fixed on `main` and
described in the release notes once a fix is available.

## Scope

Lucid is a native desktop editor. Relevant areas include:

- memory-safety bugs in document, buffer, syntax or parser code;
- path handling that escapes a project root;
- command injection through compile functions or variable expansion;
- process handling in the PTY terminal;
- unsafe deserialization of settings, caches or project files.

Build and packaging issues that require an already-compromised machine are generally out of scope.
