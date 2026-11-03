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

Please do **not** open a public issue for a security problem.

- For this private repository, contact the repository owner through an existing private channel.
  Collaborators with the necessary permissions can also create a draft security advisory in the
  repository's **Security** tab.
- If private vulnerability reporting is available and enabled after the repository becomes public,
  use **Security → Report a vulnerability**.
- Include the affected version or commit, reproduction steps, and expected impact. Avoid sharing
  sensitive details in an issue or discussion.

Maintainers will acknowledge the report as soon as possible and coordinate a fix and disclosure.

## Scope

Lucid is a native desktop editor. Relevant areas include:

- memory-safety bugs in document, buffer, syntax or parser code;
- path handling that escapes a project root;
- command injection through compile functions or variable expansion;
- process handling in the PTY terminal;
- unsafe deserialization of settings, caches or project files.

Build and packaging issues that require an already-compromised machine are generally out of scope.
