# Original Lucid feature checklist

This is the original checklist from the conversation. Its 382 unchecked boxes are the requested scope, not a statement that every item remains unimplemented. Completion must be supported by implementation and verification evidence.

Here’s the list I’d use to take Lucid from an ambitious beta to an editor people can confidently use every day. “Perfect” is a moving target, but each item describes something concrete we can build or verify.

The most important principle: **Lucid should remain Lucid**. It can become as capable as VS Code without adopting permanent tabs, a crowded sidebar, or a traditional integrated terminal.

## 1. Editing fundamentals

These are the things users expect to work so naturally that they stop thinking about them.

- [ ] Make typing, deleting, selecting, and navigating consistent across macOS, Linux, Windows, FreeBSD, and OpenBSD.
- [ ] Make double-click select a word according to the current language.
- [ ] Make repeated clicks select a line and provide predictable selection behavior for additional clicks.
- [ ] Support word navigation and word deletion with platform-standard shortcuts.
- [ ] Support movement to the beginning and end of a line, document, and paragraph.
- [ ] Support extending selections with all navigation shortcuts.
- [ ] Add multiple cursors and multiple selections.
- [ ] Add “select next occurrence” and “select all occurrences.”
- [ ] Add move line up/down and duplicate line.
- [ ] Add delete line, join lines, and insert line above/below.
- [ ] Add comment and uncomment actions for lines and blocks.
- [ ] Make indentation and outdentation work for selections and multiple lines.
- [ ] Detect indentation style per file, with clear project and user overrides.
- [ ] Support tabs, spaces, and configurable indentation width.
- [ ] Add automatic closing and surrounding of brackets and quotes.
- [ ] Make bracket matching visible without being distracting.
- [ ] Add smart backspace for indentation and paired characters.
- [ ] Add configurable line wrapping and sensible behavior for long lines.
- [ ] Make horizontal and vertical scrolling smooth and predictable.
- [ ] Keep the cursor visible after edits, navigation, and window resizing.
- [ ] Make selection, cursor, and text positions agree at every zoom level.
- [ ] Handle Unicode correctly, including emoji, combining characters, right-to-left text, and non-Latin input methods.
- [ ] Support common line endings without silently changing them.
- [ ] Detect unsupported encodings and explain the available options.
- [ ] Add configurable whitespace and end-of-line indicators.
- [ ] Make undo and redo group changes in ways that match user expectations.
- [ ] Preserve undo history when switching between open files.
- [ ] Make large-file behavior predictable, with clear limits and graceful degradation.
- [ ] Add configurable autosave that respects external changes and recovery rules.
- [ ] Add an optional minimap only if it fits Lucid’s design and is useful.

## 2. File safety and recovery

This should be one of Lucid’s strongest features.

- [ ] Never silently overwrite a file changed by another application.
- [ ] Detect external changes while a file is open and explain what happened.
- [ ] Offer reload, compare, keep local changes, and save a copy where appropriate.
- [ ] Show a clear comparison before the user resolves conflicting versions.
- [ ] Preserve unsaved work through crashes, forced quits, and system restarts.
- [ ] Restore unsaved work even when the original file has been moved or deleted.
- [ ] Keep local history with timestamps and meaningful descriptions.
- [ ] Let users preview and compare a snapshot before restoring it.
- [ ] Make recovery available without requiring users to remember a shortcut.
- [ ] Make it clear which recovery data is current and which is older.
- [ ] Avoid treating a successful save as complete until the write has actually succeeded.
- [ ] Handle full disks, permission failures, disconnected drives, and interrupted writes.
- [ ] Preserve file permissions, relevant metadata, and symbolic-link behavior safely.
- [ ] Warn before editing files that cannot be represented without loss.
- [ ] Avoid changing line endings, encoding, or a final newline unexpectedly.
- [ ] Keep backups when a multi-file operation partially fails.
- [ ] Make search-and-replace recoverable across all affected files.
- [ ] Protect recovery data from accidental deletion during cleanup.
- [ ] Offer a configurable retention policy for local history.
- [ ] Test crash recovery by terminating Lucid at different points during editing and saving.
- [ ] Test safety with network drives, external drives, and read-only folders.
- [ ] Clearly distinguish **saved**, **unsaved**, **recoverable**, and **conflicted** states without relying only on color.

## 3. Opening files and moving around without tabs

Lucid should make switching between work feel fast without permanently occupying screen space.

- [ ] Make the open-file switcher instant and keyboard-first.
- [ ] Show recently used files in the order users actually worked with them.
- [ ] Show unsaved status and full paths when filenames are duplicated.
- [ ] Search open files by name, path, and recent activity.
- [ ] Close one file, other files, or all files from the switcher.
- [ ] Confirm before closing unsaved files, with a clear save/discard/cancel choice.
- [ ] Reopen the most recently closed file.
- [ ] Preserve cursor position, selection, scroll position, and undo history when switching.
- [ ] Add back and forward navigation through editing locations.
- [ ] Support temporary previews without losing an already open file.
- [ ] Make folders navigable with keyboard and mouse.
- [ ] Keep the current folder context when creating a file.
- [ ] Support file creation, renaming, moving, and deletion with clear feedback.
- [ ] Warn when a file operation affects open files or imports.
- [ ] Handle files renamed or deleted outside Lucid.
- [ ] Offer quick access to recent projects and recent files.
- [ ] Show a clear empty state when no project or file is open.
- [ ] Make “open with Lucid” and file associations work reliably on each supported OS.
- [ ] Explain before opening binary files in their associated external application.

## 4. Project search and replace

- [ ] Search file contents across the project quickly.
- [ ] Search filenames and paths separately from file contents.
- [ ] Support case sensitivity, whole words, and regular expressions.
- [ ] Support include and exclude patterns.
- [ ] Respect ignore files, with an explicit option to include ignored files.
- [ ] Show matching lines with enough surrounding context.
- [ ] Navigate results entirely by keyboard.
- [ ] Open a result at the exact match and preserve the result list for returning.
- [ ] Allow replacement of one match, selected matches, or all matches.
- [ ] Preview every affected file before applying a project-wide replacement.
- [ ] Make exclusions clear when files are skipped or inaccessible.
- [ ] Detect files changed between preview and replacement.
- [ ] Provide a recovery path if a replacement succeeds only partially.
- [ ] Keep search responsive in large repositories.
- [ ] Support cancellation and show progress for long searches.
- [ ] Avoid loading huge or binary files into memory unnecessarily.
- [ ] Add search history and recall previous searches.
- [ ] Preserve the search query when moving between results.

## 5. Language intelligence

This is one of the largest gaps between Lucid and a mature editor.

- [ ] Make C/C++ support dependable first.
- [ ] Detect whether `clangd` is available and explain its status.
- [ ] Offer automatic installation or setup where practical.
- [ ] Detect and use `compile_commands.json`.
- [ ] Help users generate a compilation database for common build systems.
- [ ] Show useful completion suggestions at the cursor.
- [ ] Show function signatures and parameter information.
- [ ] Support go to definition and go to declaration.
- [ ] Support find references.
- [ ] Support symbol search within the file and project.
- [ ] Add safe, project-wide symbol rename with a preview.
- [ ] Show diagnostics at the relevant code location.
- [ ] Provide a navigable list of errors and warnings.
- [ ] Offer code actions and quick fixes.
- [ ] Support formatting for files and selections.
- [ ] Respect project formatting settings.
- [ ] Explain when language intelligence is starting, unavailable, or has crashed.
- [ ] Restart failed language servers safely.
- [ ] Avoid blocking typing while a language server is busy.
- [ ] Handle stale responses after a file changes or closes.
- [ ] Support Python, Rust, JavaScript/TypeScript, and other languages through language servers.
- [ ] Let users configure or add language servers without editing obscure files.
- [ ] Clearly show which features a language server actually supports.
- [ ] Test language features with real projects, not just simulated messages.

## 6. Compile comments and running code

This could become Lucid’s signature feature if it is both powerful and easy to understand.

- [ ] Document the compile-comment syntax with examples inside Lucid.
- [ ] Highlight valid commands and explain invalid ones at the relevant line.
- [ ] Make running a command discoverable from the code that defines it.
- [ ] Show output attached to the corresponding command or function.
- [ ] Make output selectable, searchable, and copyable.
- [ ] Show whether a command is running, succeeded, failed, or was cancelled.
- [ ] Show execution time and exit status.
- [ ] Jump from compiler errors to the correct file and line.
- [ ] Recognize common compiler and test output formats.
- [ ] Rerun the last command or a failed test quickly.
- [ ] Compare the latest run with the previous run.
- [ ] Preserve enough output history to investigate intermittent failures.
- [ ] Allow cancelling a running command and its child processes.
- [ ] Handle interactive programs clearly when input is required.
- [ ] Support working directories and environment variables.
- [ ] Support common build systems such as CMake, Make, Ninja, and Cargo.
- [ ] Make executable paths and missing dependencies easy to diagnose.
- [ ] Avoid freezing the interface during a build.
- [ ] Handle huge output without exhausting memory.
- [ ] Make commands behave consistently across supported platforms.
- [ ] Warn clearly before running a command from an untrusted project.

## 7. Debugger

- [ ] Finish the basic C/C++ debugging workflow on macOS, Linux, and Windows.
- [ ] Provide a clear setup flow for missing debug adapters.
- [ ] Support launch arguments, environment variables, and working directories.
- [ ] Support attaching to an existing process.
- [ ] Set, remove, enable, and disable breakpoints.
- [ ] Support conditional breakpoints and logpoints.
- [ ] Show the current execution line clearly.
- [ ] Support continue, pause, step over, step into, and step out.
- [ ] Show the call stack and let users navigate it.
- [ ] Show local variables, arguments, and their values.
- [ ] Expand structured values and inspect arrays and objects.
- [ ] Evaluate an expression safely when the debugger supports it.
- [ ] Let users choose the active thread and stack frame.
- [ ] Show program output alongside the relevant debugging session.
- [ ] Handle a crashed or disconnected adapter without leaving Lucid stuck.
- [ ] Save useful launch configurations per project.
- [ ] Explain unsupported configurations rather than silently doing nothing.
- [ ] Allow attaching a debugging session or finding to a task.
- [ ] Test against real debug adapters and real programs on every claimed platform.
- [ ] Make AI explanations optional and clearly separate from debugger facts.

## 8. Git and code review

- [ ] Show changed, staged, untracked, and conflicted files.
- [ ] Preview changes inline in Lucid’s design language.
- [ ] Stage and unstage individual files.
- [ ] Stage and unstage selected hunks.
- [ ] Create commits with a clear summary and optional description.
- [ ] Show commit history and inspect previous changes.
- [ ] Compare a file or project against another commit or branch.
- [ ] Create, switch, rename, and delete branches safely.
- [ ] Fetch, pull, and push with clear progress and errors.
- [ ] Explain authentication failures and missing credentials.
- [ ] Handle common merge conflicts with a clear three-way view.
- [ ] Show which conflict sections remain unresolved.
- [ ] Support common merge, rebase, and cherry-pick workflows with explicit safeguards.
- [ ] Make it clear before an operation can discard or rewrite work.
- [ ] Show the author and commit associated with lines when requested.
- [ ] Support GitHub pull-request information and review comments where authorized.
- [ ] Make remote-review limitations clear when a provider is unavailable.
- [ ] Handle repositories with submodules and worktrees.
- [ ] Keep Git operations responsive in large repositories.
- [ ] Test against real repositories, not only mocked Git output.

## 9. Local AI and optional API AI

The AI should help users understand and improve their code while leaving them in control.

- [ ] Make the local model setup visible and understandable.
- [ ] Show download size, progress, verification, and installation status.
- [ ] Allow users to cancel or retry a failed download.
- [ ] Explain storage and memory requirements before downloading.
- [ ] Detect when the local model cannot run on the current machine.
- [ ] Allow choosing an appropriate lightweight local model.
- [ ] Make the recommended setup as automatic as practical.
- [ ] Avoid requiring a separate application such as Ollama or LM Studio.
- [ ] Show whether each AI action uses a local model or a remote provider.
- [ ] Allow selecting the provider and model separately for each AI feature.
- [ ] Store API keys using the operating system’s credential storage where supported.
- [ ] Never place API keys in logs, project files, or crash reports.
- [ ] Explain what project content will be sent before a remote request.
- [ ] Provide clear errors for invalid keys, unavailable models, and network failures.
- [ ] Allow stopping a generation.
- [ ] Keep responses selectable, scrollable, and readable.
- [ ] Explain selected code.
- [ ] Explain compiler and test errors.
- [ ] Suggest a focused fix without applying it automatically.
- [ ] Preview changes before accepting them.
- [ ] Refuse to apply an outdated suggestion when the underlying code changed.
- [ ] Improve readability while following the surrounding file’s style.
- [ ] Teach the AI about Lucid’s compile-comment syntax.
- [ ] Make AI help available from debugger failures and tasks where useful.
- [ ] Show progress when a model is loading, downloading, or generating.
- [ ] Set sensible context limits and explain when content was omitted.
- [ ] Test local inference and remote-provider errors on supported platforms.

## 10. Tasks and project coordination

- [ ] Create, edit, complete, reopen, and delete tasks.
- [ ] Support subtasks with clear completion state.
- [ ] Keep the three priority tiers understandable and consistent.
- [ ] Record who created a task and who is working on it.
- [ ] Record who completed or reopened a task or subtask.
- [ ] Allow assignment to one or more collaborators.
- [ ] Add descriptions, comments, and links to relevant files or lines.
- [ ] Attach debugger findings, compile output, and commits to tasks.
- [ ] Keep a history of how an issue was investigated and solved.
- [ ] Search and filter tasks by priority, status, owner, and text.
- [ ] Support task dependencies where useful.
- [ ] Avoid losing task changes when switching projects.
- [ ] Make task data portable with the project where appropriate.
- [ ] Handle concurrent task edits safely once multiplayer is available.
- [ ] Provide a clear export or backup format.

## 11. Multiplayer and shared projects

This is a major feature and should not be presented as finished until it is genuinely usable.

- [ ] Integrate hosting and joining into the editor.
- [ ] Support both a user-hosted project and a separately hosted service.
- [ ] Make project browsing and opening work for remote participants.
- [ ] Show each collaborator’s cursor and selection.
- [ ] Identify collaborators by more than color alone.
- [ ] Synchronize edits without losing local work.
- [ ] Detect conflicting changes and provide an understandable resolution flow.
- [ ] Preserve unsent work when the connection drops.
- [ ] Reconnect and resynchronize without silently overwriting edits.
- [ ] Provide durable recovery for unsaved host and participant changes.
- [ ] Explain who can edit, save, or invite others.
- [ ] Support read-only participants.
- [ ] Allow ending a session or removing a participant.
- [ ] Protect share links and explain that they grant access.
- [ ] Require an encrypted connection before exposing the feature.
- [ ] Support a trusted private network such as Tailscale where appropriate.
- [ ] Implement and test Windows transport.
- [ ] Test latency, packet loss, disconnects, and simultaneous edits.
- [ ] Handle large projects without freezing the host.
- [ ] Prevent a host shutdown from silently discarding unsaved shared changes.
- [ ] Document the limitations and security model clearly.

## 12. Modules

- [ ] Make the GitHub catalog reliable and easy to browse.
- [ ] Show loading, download progress, and useful failure messages.
- [ ] Preserve the last loaded catalog when a refresh fails.
- [ ] Search installed and available modules.
- [ ] Preview a module before installing it.
- [ ] Show author, version, description, source, and requested permissions.
- [ ] Make installation and enabling separate, understandable actions.
- [ ] Let users disable and remove modules easily.
- [ ] Support manually adding modules from a local file or package reference.
- [ ] Verify package integrity before installation.
- [ ] Clearly distinguish trusted and unverified sources.
- [ ] Provide a real permission model before adding modules that access files, networks, processes, or settings.
- [ ] Prevent a faulty module from freezing or crashing the editor.
- [ ] Add a module API for useful editor integrations.
- [ ] Allow modules to contribute temporary surfaces and keyboard actions.
- [ ] Provide a developer guide and sample modules.
- [ ] Support updates, compatibility information, and rollback.
- [ ] Explain when an installed module is incompatible with the current Lucid version.
- [ ] Establish how third-party modules can be discovered and published.

## 13. Settings and customization

- [ ] Make every setting understandable without reading the source code.
- [ ] Organize settings into a consistent full-page experience.
- [ ] Add settings search.
- [ ] Explain defaults and allow individual settings to be reset.
- [ ] Support user-wide and project-specific settings where appropriate.
- [ ] Show when a project overrides a user setting.
- [ ] Apply visual settings immediately when safe.
- [ ] Explain when a restart is required.
- [ ] Protect against overwriting settings changed by another process.
- [ ] Make font family, size, line height, and spacing adjustable.
- [ ] Support themes and primary colors consistently across the interface.
- [ ] Ensure icons respond correctly to the chosen theme or accent.
- [ ] Support the background scenes and their color modes reliably.
- [ ] Provide an option to reduce or disable animations.
- [ ] Make background blur and contrast work well on every supported renderer.
- [ ] Allow configuring shortcuts and detecting conflicts.
- [ ] Provide import and export for settings.
- [ ] Keep secret credentials out of exported settings.

## 14. Visual polish and interaction details

- [ ] Make all menus behave consistently with keyboard, mouse, and trackpad.
- [ ] Make transitions smooth without slowing down frequent actions.
- [ ] Reduce excessive bounce or motion.
- [ ] Add subtle indicators when a menu contains more scrollable content.
- [ ] Keep text selection aligned with the displayed text.
- [ ] Prevent stale hover or selection highlights after a menu changes.
- [ ] Make loading states visible without clutter.
- [ ] Make error messages actionable and place them near the relevant action.
- [ ] Ensure every action has clear feedback when it succeeds or fails.
- [ ] Keep focus predictable after opening and closing temporary surfaces.
- [ ] Make Escape consistently cancel or close the current surface.
- [ ] Preserve context when moving between related menus.
- [ ] Ensure long names and paths are readable without breaking layouts.
- [ ] Handle very small and very large window sizes.
- [ ] Test high-DPI displays and mixed-scale multi-monitor setups.
- [ ] Keep animations responsive at different frame rates.
- [ ] Offer reduced-motion behavior.
- [ ] Ensure animated backgrounds never interfere with text readability.
- [ ] Keep visual effects efficient enough for laptops and integrated GPUs.
- [ ] Review every empty state, confirmation, warning, and loading state.

## 15. Accessibility

- [ ] Make all essential actions available by keyboard.
- [ ] Provide visible focus indicators.
- [ ] Expose editor content and controls to platform screen readers.
- [ ] Announce errors, status changes, and completion suggestions appropriately.
- [ ] Make cursor and selection information accessible.
- [ ] Support adjustable text size and interface scaling.
- [ ] Provide high-contrast options.
- [ ] Avoid relying on color alone for errors, Git state, or collaborators.
- [ ] Support reduced motion.
- [ ] Test common screen readers on each platform.
- [ ] Test with keyboard-only navigation across every temporary surface.
- [ ] Make hit targets comfortable without wasting space.
- [ ] Verify contrast over every background theme.
- [ ] Support system accessibility preferences where possible.
- [ ] Document keyboard and accessibility features in one discoverable place.

## 16. Performance and large projects

- [ ] Keep typing responsive under load.
- [ ] Measure startup time, file-open time, search latency, and memory use.
- [ ] Avoid scanning entire projects repeatedly for routine actions.
- [ ] Make indexing incremental and cancellable.
- [ ] Respect ignored directories and large generated folders.
- [ ] Handle repositories with hundreds of thousands of files gracefully.
- [ ] Avoid blocking rendering on disk, network, or language-server work.
- [ ] Limit memory used by output, diagnostics, search, and history.
- [ ] Degrade gracefully when a file or project exceeds supported limits.
- [ ] Keep animation and GPU use reasonable when the window is inactive.
- [ ] Test on slower CPUs, limited-memory devices, and integrated graphics.
- [ ] Add repeatable performance tests and track regressions.

## 17. Platform integration and distribution

- [ ] Produce working builds for macOS, Linux, Windows, FreeBSD, and OpenBSD.
- [ ] Be precise about which platforms are built versus actually tested.
- [ ] Provide a polished macOS DMG.
- [ ] Provide a Windows MSI and a standalone build.
- [ ] Provide Linux packages and an install command with clear behavior.
- [ ] Provide appropriate BSD packages or install instructions where practical.
- [ ] Keep standalone downloads available.
- [ ] Test clean installation and removal on each platform.
- [ ] Handle upgrades without losing settings or recovery data.
- [ ] Register file associations and “Open with Lucid” correctly.
- [ ] Support opening files from the OS while Lucid is already running.
- [ ] Sign and notarize builds where applicable and available.
- [ ] Publish checksums and clear release notes.
- [ ] Explain system requirements and dependencies.
- [ ] Verify that installation scripts fail clearly and do not hide errors.
- [ ] Test different desktop environments and display scaling on Linux.
- [ ] Validate debugger, AI, rendering, and file safety separately on each platform.

## 18. Trust, security, and privacy

- [ ] Explain what runs locally and what may use the network.
- [ ] Treat unfamiliar projects as untrusted until the user permits code execution.
- [ ] Clearly identify commands that will execute processes.
- [ ] Protect API keys and collaboration links.
- [ ] Review module installation and permissions carefully.
- [ ] Validate downloaded models and packages.
- [ ] Avoid logging private code or credentials unnecessarily.
- [ ] Make crash reports opt-in and explain what they contain.
- [ ] Provide a security reporting process.
- [ ] Regularly update and audit dependencies.
- [ ] Test path handling, symbolic links, archive extraction, and malformed files.
- [ ] Ensure recovery and local history have sensible privacy and retention settings.
- [ ] Document the limitations of multiplayer and remote AI honestly.

## 19. Testing and release quality

- [ ] Keep the full automated test suite passing.
- [ ] Run CI on macOS, Linux, and Windows for every significant change.
- [ ] Add BSD build coverage where infrastructure permits.
- [ ] Test actual app startup and core workflows, not only compilation.
- [ ] Test clean installs and upgrades.
- [ ] Test the complete first-run experience.
- [ ] Test on machines without preinstalled developer tools.
- [ ] Test network failures and offline behavior.
- [ ] Test low disk space and permission errors.
- [ ] Test large files, large projects, and unusual filenames.
- [ ] Test crash recovery and external file modifications.
- [ ] Test native keyboard shortcuts on different keyboard layouts.
- [ ] Test screen readers and keyboard navigation.
- [ ] Keep a documented manual release checklist.
- [ ] Record which platforms and workflows were actually validated.
- [ ] Keep release notes honest about known limitations.
- [ ] Avoid calling a feature finished solely because its underlying code compiles.

## 20. Documentation and first impressions

- [ ] Explain what Lucid is and who it is for in the first few sentences.
- [ ] Provide a quick-start guide for each supported OS.
- [ ] Explain the tab-free navigation model.
- [ ] Teach the compile-comment workflow with short examples.
- [ ] Make shortcuts discoverable in the app and searchable in documentation.
- [ ] Explain local AI setup and remote-provider privacy.
- [ ] Provide a C/C++ project setup guide.
- [ ] Document debugging, Git, tasks, recovery, and modules.
- [ ] Show common troubleshooting steps with clear error messages.
- [ ] Keep screenshots and feature claims current.
- [ ] Include an honest feature and platform support matrix.
- [ ] Provide a simple way to report bugs and request features.
- [ ] Make the website’s downloads, documentation, and legal information complete.
- [ ] Ensure the app never advertises an unfinished feature as fully working.

## What I would build first

To make the biggest difference before adding more surface area, I’d prioritize:

1. **File safety and recovery** — people must trust Lucid with their work.
2. **Editing and navigation** — the basics need to feel effortless every day.
3. **Complete C/C++ language intelligence** — especially setup, diagnostics, navigation, rename, and formatting.
4. **Build output and debugger reliability** — make the compile-comment workflow genuinely useful on real projects.
5. **Git workflows** — particularly staging, commits, branches, and conflict resolution.
6. **Cross-platform validation** — build and actually exercise the same workflows on the platforms we claim to support.
7. **Modules and AI polish** — useful onboarding, clear status, and safe failure behavior.
8. **Multiplayer** — finish it as a substantial dedicated project, including recovery and security.

Once those are dependable, Lucid would be much closer to being a serious daily driver. Its opportunity to become *better for its users* is then the combination of reliability, a clean interface, excellent code-attached workflows, and helpful AI that stays under the user’s control.
