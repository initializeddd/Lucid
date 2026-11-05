# Lucid feature progress audit

This is a working inventory for the 20-category improvement checklist. A category
is **partial** until each requested behavior has implementation evidence and has
been exercised on its applicable platforms. Counts of code changes or passing
tests are not counts of completed checklist items. The [original checklist](original-feature-checklist.md)
contains 382 checkboxes, including overlapping requirements; the earlier “360”
count was an estimate. The boxes in that preserved list have not yet been audited
individually against the current implementation.

| Checklist area | Current evidence | Material work still needed |
| --- | --- | --- |
| Editing fundamentals | `Document`, editor interaction tests, keyboard shortcuts, language-specific line and block comment delimiters, and paired deletion of automatically inserted JSON quotes | Syntax-aware comment context, configurable language mappings, multiple cursors, broader language-aware pairing, complete Unicode and platform testing |
| File safety and recovery | guarded project writes, local snapshots, recovery tests, and preservation of dirty inactive buffers when their disk versions change | Full crash and failure coverage, complete recovery and cross-platform validation |
| Tab-free navigation | file switcher, reopen closed file, interaction tests | Additional close operations and durable navigation history |
| Project search and replace | reviewed and selective literal replacement, whole-word and ASCII case-insensitive search, path include/exclude patterns, optional generated directories, cancellation | Unicode case folding, regex search, richer glob semantics, ignore-file semantics |
| Language intelligence | clangd client, diagnostics, navigation, symbol search, signatures, capability status, reviewed rename and guided CMake database generation; macOS clangd test covers a source/header rename | Full previews for long changes, broader project and platform validation, Windows CMake setup, automatic server installation, more languages and dynamic capability registration |
| Compile comments | parser, subprocess output, process tests | Broader real-project coverage, Windows execution, structured failure navigation |
| Debugger | DAP session, launch arguments/environment, pause command with protocol tests and macOS LLDB interaction test, macOS LLDB multithread integration test | Attach, richer launch configuration, and Linux/Windows validation including pause |
| Git and review | File staging, commits, remote operations and reviewed local branch switching; real Git tests cover stale branch tips and changes outside a project subdirectory, and macOS interaction coverage checks explicit confirmation and open-file recovery | Hunk staging, branch creation/rename/deletion, merge and pull-request workflows; Linux and Windows runtime validation |
| AI | local-model and OpenRouter integration | Broader runtime testing, model selection and lifecycle polish |
| Tasks | task board and persistence tests | Full collaboration and task-to-workflow integration |
| Multiplayer | internal session, replica and transport tests | Editor integration, durable recovery, encryption and Windows transport; see `collaboration-design.md` |
| Modules | declarative modules, catalog, preview and tests | Broader API, permission enforcement, compatibility and update lifecycle |
| Settings | full settings page and interaction tests | Search, project overrides, shortcut configuration and import/export |
| Visual polish | renderer and interaction tests | Full visual review across themes, displays and platforms |
| Accessibility | macOS accessibility bridge and keyboard support | Screen-reader support and end-to-end testing on other platforms |
| Performance | bounded output and undo history, asynchronous search | Large-project benchmarks, incremental indexing and broader memory limits |
| Platform integration | packaging scripts and build workflows | Clean-install and runtime validation on all claimed systems |
| Trust, security and privacy | credential storage, checksums and guarded operations | Project trust, module sandboxing and collaboration security review |
| Testing and release quality | automated suite and selected macOS integration tests | Cross-platform UI, failure, accessibility and clean-install coverage |
| Documentation and first impressions | README, guides and support matrix | Keep all documentation aligned with finished, validated behavior |

The macOS suite and selected integration tests have passed in the current working
tree, but this does not establish completion of the checklist or prove behavior
on Linux, Windows, FreeBSD, or OpenBSD. Update this audit as individual features
gain implementation and platform-specific evidence.
