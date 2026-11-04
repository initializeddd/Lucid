# Security policy

## Supported versions

Lucid is pre-1.0. Security fixes are applied to the latest `main` branch and the most recent
published beta release.

| Version | Supported |
| --- | :---: |
| Most recent published beta | ✅ |
| `main` | ✅ |
| Older releases | ❌ |

## Reporting a vulnerability

Please do **not** open a public issue for a security problem.

Use GitHub's private vulnerability reporting:

1. Open the repository's **Security** tab.
2. Choose **Report a vulnerability**.
3. Include the affected version or commit, reproduction steps and expected impact.

Maintainers will acknowledge the report as soon as possible and coordinate a fix and disclosure.
If the report button is unavailable, contact a maintainer privately through an existing private
channel and ask how to report it. Do not include vulnerability details in a public message.

Please avoid sharing sensitive details in an issue or discussion.

## Scope

Lucid is a native desktop editor. Relevant areas include:

- memory-safety bugs in document, buffer, syntax or parser code;
- path handling that escapes a project root;
- command injection through compile functions or variable expansion;
- process handling in the PTY terminal;
- unsafe deserialization of settings, caches or project files;
- vulnerabilities in the local model download and verification path.

Build and packaging issues that require an already-compromised machine are generally out of scope.

## Disclosure

We aim to publish a fix and an advisory once a report is confirmed. Reporters are credited in the
advisory unless they ask to remain anonymous.
