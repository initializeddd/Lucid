# Lucid modules (preview)

Lucid modules are declarative manifests that invoke Lucid-provided commands.
Each module declares the project, network, process, settings, and device access its host
actions require. Lucid shows these permissions before installation; actions run
only when selected, and network actions ask for confirmation before sending.
Some actions run selected programs or tests, which can themselves access the
network. There is no arbitrary-code module runtime. The GitHub catalog lists
modules published in Lucid's catalog package; it is not a global index of every
GitHub package. The legacy catalog and starter package are publicly downloadable
without a GitHub account; new packages must be made public before anonymous use.

Open **Run > Modules** or press **Command+Shift+M** (Ctrl+Shift+M on
Windows/Linux). Choose **Browse GitHub modules** to load the catalog, or
**Add module** to enter a local manifest path or a public package reference in
the form `ghcr.io/owner/package:version`. Lucid shows the module's commands and
requested permissions before installing it. Installation never overwrites an
existing module and new modules start disabled. Downloads require HTTPS and
the package layer's SHA-256 digest is checked before use.

To publish a module, package its `module.json` as the single OCI layer with
media type `application/vnd.lucid.module.v1+json`. Catalog entries contain an
`id`, a GitHub Container Registry `package` (`owner/name`), and a `version`;
optional `name` and `description` fields make entries easier to find.
The catalog is an OCI package with one layer of type
`application/vnd.lucid.catalog.v1+json`. See the starter module and
`.github/workflows/publish-modules.yml` for examples. The publishing workflow
validates and publishes each checked-in `modules/<id>/module.json`, then
generates the catalog from those manifests. Capability-aware builds use the
`v2` catalog tag, falling back to the legacy `latest` catalog when it is unavailable;
publishing does not overwrite `latest`. Modules may declare a `requires` array,
such as `snippet-v1` or `project-secret-scan-v1`, and Lucid prevents installation
when a required capability is unavailable. This cannot retrofit compatibility checks
into older clients, which continue to use the legacy catalog. New GitHub packages must be made
public before anonymous downloads work. GitHub package listing requires
authentication, so packages outside this catalog can be added manually by
reference.

Alternatively, create a folder under Lucid's `Modules` directory, named with lowercase ASCII
letters, digits, hyphens, or underscores. Its location is:

- macOS: `~/Library/Application Support/Lucid/Modules/`
- Linux: `${XDG_CONFIG_HOME:-~/.config}/Lucid/Modules/`
- Windows: `%APPDATA%\Lucid\Modules\`

Place a `module.json` in the folder, for example:

```json
{
  "name": "Example snippets",
  "version": "1.0.0",
  "permissions": {
    "project": false,
    "network": false,
    "processes": false,
    "settings": false
  },
  "commands": [
    { "title": "Insert TODO", "insert": "TODO: " }
  ]
}
```

Open **Run > Modules** on macOS or press **Command+Shift+M** (Ctrl+Shift+M on
Windows/Linux). Select a module to inspect its requested permissions, enable or
disable it, remove its folder, or run one of its commands. New modules start
disabled. Use arrows or Tab to move, Enter to select, and Escape to return.
Running a command replaces the current selection, if any; the edit can be undone.
To create a personal snippet, select up to 4096 bytes of text in the editor, open
Modules, choose **Save selection as snippet**, and name it. Lucid saves it locally
as an enabled personal module; its contents are inserted literally when run.
Alternatively, choose **Save selection as template** to interpret placeholder,
selection and cursor markers in the selected text. The template is validated
before it is saved and installed as a personal module.
Commands with `"snippet": true` may use `{{selection}}` to insert the selected text and a single
`{{cursor}}` to place the caret after insertion. Expanded snippets are limited to
1 MiB. Commands without that flag insert all text literally, including markers.
Markers do not execute code. Snippet markers require a Lucid version that supports
the `snippet` flag; older builds insert them literally.
With the `snippet-placeholder-v1` capability declared, a snippet may instead use
one `{{placeholder:default text}}` marker. Lucid selects the default text after
inserting it so typing replaces it. The newer `snippet-placeholders-v2` capability
supports up to 32 fields and an optional final `{{cursor}}`. Tab moves to the next
field, Shift+Tab moves back, and Escape ends navigation. Navigation ends if the
document changes outside the active field. The optional `snippet-indent-v1` capability
prefixes subsequent inserted lines, including lines from a wrapped selection, with
the insertion line's leading whitespace while keeping field and cursor positions aligned.
For documents over 16 MiB, the template
is inserted but field navigation is unavailable. With the `snippet-linked-v1`
capability and `snippet-placeholders-v2`, define a linked field with
`{{linked:name:default}}` and refer to it later with `{{linked:name}}`. Edits
mirror to all occurrences as one undoable edit. Tab visits each linked name
once; undo, redo or editing outside the active field ends navigation. References
must follow a single definition, and a snippet may contain at most 32 field
occurrences.
The Local Error Explainer sends the current diagnostic with up to five bounded nearby
source lines, or a bounded debugger state (including available stack frames and variables), to Lucid's local model after the
user confirms. It does not send that context to OpenRouter, even when other AI actions
use that provider. Source and variable values may contain sensitive data. If the
document changes before an explanation starts, select the diagnostic again.
Lucid may download the local model on first use.
Removal deletes the entire module folder after confirmation. Manifest changes are
rechecked before commands run. To change a manifest, disable the module first and
then reopen the manager.

A manifest is limited to 64 KiB and 32 commands; command text is limited to 4096
bytes. Modules requesting unsupported access stay disabled. The secret scan reads
saved indexed files, omits binary files, and detects a limited set of patterns;
it does not inspect unsaved buffers or guarantee that a project contains no secrets.
Its results show file locations without reproducing the matching secret. This
format is not compatible with VS Code extensions.

When the Secret Scanner module is enabled, Lucid also scans the full contents of
staged files during its commit review, using the reviewed Git tree rather than the
working files. Binary files, non-regular entries and blobs above Git's 2 MiB review
limit make the scan incomplete. Findings and incomplete scans require an explicit
choice to continue; Git checks the staged tree again before committing. This is a
pattern check, not a guarantee, and commits made outside Lucid are not covered.
In a project scan, filter findings by AWS, GitHub or private-key pattern. Results
show a category and location without displaying the matched value; opening a
finding reveals the file contents. Navigation checks that the file still matches
the scanned bytes and requires dirty open documents to be resolved. Skipped files
and scan limits make the result incomplete, and the supported patterns do not
cover every credential.
The all-findings view and staged scan also detect GitLab personal, project and
group access tokens with the default `glpat-` prefix and 20-character suffix.
GitLab instances can use custom prefixes, which this pattern does not detect.
Private-key header detection includes PEM, OpenSSH and PGP private-key blocks;
public-key blocks do not produce this finding.

The CMake Navigator reads an existing CMake File API codemodel from a selected
project-relative build directory and shows targets, source files, compile groups,
target dependencies and reported output artifact paths, as well as
Boolean and supported text/path cache options. Dependencies and artifact paths are
read-only declarations from the last configuration; output existence is not checked.
Text and path values are checked
against simple declared STRINGS choices when present; complex escaped choices are
unsupported. Values are reviewed before a conflict-checked write, paths are not
verified, and no CMake reconfigure occurs automatically. **Prepare CMake query** creates the codemodel and cache query
files without overwriting existing ones;
reconfigure the build afterwards, then choose **Browse CMake targets**. **Edit CMake
cache options** lets you select a Boolean option and explicitly confirm an ON/OFF cache
change. Lucid checks that the cache still matches the reviewed value and refuses
conflicts; the File API view becomes stale and the build must be reconfigured before
building. A separate process can still race the final check and write. The module
can build a selected File API target after you choose an absolute CMake executable,
review its exact arguments and explicitly confirm. CMake, build tools and project
scripts run with your privileges and can change files; cancellation can leave changes.
Open dirty documents must be resolved first. It does not support build directories
outside the project, and other build inputs or the executable can change after review.

Release-Note Builder reads saved project tasks and up to 50 recent local Git commits.
Open a Markdown document, choose entries in the module, filter to selected items,
optionally set a release version and date, and preview the generated Markdown before inserting the exact reviewed draft at
the cursor. The inserted draft is editable. No entry is selected automatically. Conventional `feat:` and
`fix:` subjects are grouped as features and fixes; other commits are changes. Git
must be installed for history, and the module reports unavailable or truncated
sources. It does not publish a release or fetch pull requests.

Web Component Preview currently renders a reduced snapshot up to 1024 pixels per side on macOS
only. Open a JSON configuration such as
`{"entry":"index.html","assets":["index.html","app.js"],"width":800,"height":600,"state":"?mode=dark"}`
and choose **Render web preview**. Asset paths are project-relative and only
listed files are copied into the preview; the optional state is a query or fragment.
Confirm only for trusted code: JavaScript executes in WebKit and network isolation
is not guaranteed. Rendering readiness is heuristic. Interactive previews, Linux
and Windows runtimes, and full-resolution viewing are not yet supported.

Developers can run the optional WebView2 smoke tests on an interactive Windows
desktop with the WebView2 Runtime, Python, and the Lucid build dependencies
installed. From the repository root, use a new or empty build directory:

```powershell
python tools/webview2/fetch_sdk.py build-webview2-sdk --arch x64
powershell -ExecutionPolicy Bypass -File tools/webview2/run_smoke.ps1 -SdkDirectory build-webview2-sdk -Architecture x64
```

Choose matching `x86` or `arm64` values in both commands when appropriate; pass
`-BuildDirectory` to select a different empty build directory. The SDK fetcher
checks the pinned package hash; it does not install the WebView2 Runtime. These
opt-in tests exercise the platform adapters and selected rendering and network
checks, not the full editor UI or a security sandbox. Project code must be
trusted and can access the network. Windows preview remains disabled until it
has been verified on Windows; uncertain shutdown may leave a temporary profile.

Markdown Studio can navigate ATX (`#`) and Setext headings in the current
editor buffer, including unsaved text, while ignoring fenced code blocks. Its text preview shows
headings, paragraphs, lists, quotes and code without executing raw HTML or loading
remote images. Inline styling and table layout are simplified, and tables remain
text. The local link checker checks project-contained file and directory targets
without making network requests. Filter missing or unchecked links and select a
result to navigate to its label when a reliable source location is available;
changed buffers must be checked again. Simple ASCII heading fragments are checked
against the current buffer or a project-local Markdown file, including repeated
heading suffixes. Markup, punctuation, non-ASCII headings, custom HTML anchors,
and other uncertain fragment cases are labelled **Not checked**, as are external
links and unsupported encoded paths. Fragment checking is bounded and depends on
the target renderer using compatible heading IDs. Headings
with text inside lists or blockquotes are included in the outline; empty nested
headings are not currently listed.

The separate **Check an external link** command requires network permission. It
lists HTTP(S) links without displaying their destinations; select one, inspect it
in the source, and confirm each request. Lucid sends the selected URL path to its
host using HEAD, without following redirects or using a proxy. Credential-bearing
URLs, query strings and fragments are refused; private and reserved network
addresses are blocked. A URL path can itself contain sensitive information, so
review it before confirming. Cancellation, timeouts and response limits apply.
Servers may reject HEAD or return inconclusive statuses, and successful checks do
not verify fragments or future availability.

Visual Regression Review compares two project-local PNG snapshots, reports changed
pixels and shows reduced baseline, candidate and difference previews. The preview
is limited to 1024 pixels per side; inspect original images for fine details. An
explicit confirmation replaces the baseline with the reviewed candidate after
checking that both files are unchanged. A cross-process change between the final
check and replacement remains possible. **Compare with configuration** reads a
project-relative JSON file, for example `{"version":1,"threshold":10,"masks":[{"x":0,"y":0,"width":20,"height":10}]}`.
Threshold is 0–255 per channel; masks are pixel rectangles validated against both
images. Missing, malformed, oversized or out-of-bounds configurations are reported.
The default comparison uses zero threshold and no masks. Full-resolution preview
and a graphical configuration editor are not yet available.

Pull-Request Companion accepts a GitHub pull request URL and asks before
contacting GitHub. It can fetch anonymously or, if explicitly selected, use the
`GH_TOKEN` environment variable; the token is not displayed or saved by the
module. It shows bounded PR metadata, files, review comments, conversation comments and check runs,
with incomplete results flagged,
and selecting a file opens its provided patch with old and new line numbers.
Conversation comments are read-only; displayed text is limited to 1,024 bytes
and the overview may omit entries beyond its display limit.
Patches are limited to 1000 displayed lines, and incomplete or unsupported
hunks are reported instead of being shown as complete. Select a new-side line
to draft an inline review comment, then explicitly confirm sending with
`GH_TOKEN`. You can also draft an approval, request changes, or submit a comment
review, then inspect the decision, message, pull request and head commit before
confirming. Lucid rechecks the head before submission and sends the reviewed
commit ID; GitHub can still change between the check and submission. A failed or
cancelled request may already have succeeded; refresh before retrying. Browsing
fetching missing patches, token storage and GitHub Enterprise
hosts are not supported.

Container Tools asks for an absolute Docker or Podman executable path. After a
separate confirmation, it can list all containers on that runtime's configured
host or filter by an exact `com.docker.compose.project.working_dir` label matching
the open project's path. This label identifies Compose working directories, not
all containers related to a project; custom paths, alternate providers, and remote
hosts with different paths may result in missing matches. The filter sends the
project path to the selected runtime. Selecting a container and
confirming again reads its most recent 200 log lines. Runtime execution can
contact a remote host, and logs may contain secrets. Start, stop, and restart
each require a separate confirmation identifying the container, and operate on
the runtime's configured host. A completed action clears the listing so it can
be refreshed. Output and runtime duration are bounded; cancellation cannot undo
an action already accepted by the runtime.
**Inspect container details** separately runs the selected runtime to show image,
state, health, published ports and mounts. The runtime's full inspect response
can contain secrets and is held temporarily in memory; environment variables and
credentials are not displayed or retained in the parsed view. Mount paths and
addresses can themselves be sensitive. Unsupported or limited metadata is marked
incomplete, and results are a snapshot that can become stale.

**Configured project actions** reads a project-relative JSON file, for example
`{"version":1,"actions":[{"name":"Build web","operation":"build","composeFile":"compose.yaml","services":["web"]}]}`.
Operations are `build`, `up`, `stop` and `restart`, with one to sixteen named
services and no arbitrary command-line flags. Lucid shows the selected runtime,
operation, Compose file and services before a separate confirmation. The action
runs with the user's privileges and may execute project-defined code, access other
files or contact a remote host. The configuration and Compose file are checked
again before launch, but they or referenced files can change while the runtime
reads them. Output is bounded and may contain secrets; cancellation does not undo
changes already accepted by the runtime.

**Edit request templates** adds a placeholder GET request, renames a request, or
removes one after confirmation in the open JSON document without resolving variables.
Changes remain unsaved and can be undone. Edit a template's URL, headers and body
directly in the document. Lucid rejects edits if the document or module changes
while the template editor is open.

API Request Studio reads the open JSON collection, including unsaved edits. The
collection contains a `requests` array with `name`, `method`, `url`, optional
`headers` and `body`, and an optional `variables` object of string values used as
`{{NAME}}` in URLs, headers and bodies. Select a request and explicitly confirm
its destination before Lucid sends it. Escape cancels an in-flight request. The
response display is bounded, with separate status, headers and body views and elapsed
transport time. Connection, DNS, timeout, TLS and interrupted-response failures are
reported without echoing URLs or credentials; redirects
and proxies are not followed. Response
headers and bodies may contain sensitive data. **Open collection with environment**
reads a chosen project-relative JSON file such as `{"environments":[{"name":"dev","variables":{"TOKEN":{"type":"string","value":"example","secret":true},"PORT":{"type":"number","value":"8080"}}}]}`.
Variable types are `string`, `number` (encoded as a JSON string), and `boolean`
(a JSON boolean); profile values override collection variables. Secret-marked values
are hidden in the picker and cannot appear in the displayed destination, but may
be sent in request headers, paths or bodies. All values remain plaintext in files
and memory; this is not a credential vault. Protect profile files and avoid committing
secrets. After a complete response, **Save response body** creates a new project-local
file after explicit confirmation; existing files are not overwritten. The exact body
may be binary and may contain sensitive data, so protect the saved file. Transport errors
and incomplete responses cannot be exported. Binary responses, large responses and more
than 200 displayed lines are limited in the preview.

Diagram-as-Code Preview renders the current `.mmd` or `.mermaid` buffer locally,
including unsaved edits. It accepts a small Mermaid flowchart subset: directed
graphs, including cycles, self-loops and bidirectional `<-->` links, with plain node labels and simple edge labels such as `A -->|yes| B`,
with one node or edge chain per line (for example, `A --> B -->|yes| C`).
Branch groups such as `A & B --> C & D` expand to all four links; members of
one group must be unique. Rounded `A(label)`, stadium `A([label])` and decision
diamond `A{label}` nodes are supported; other custom shapes and styling are
unsupported. Parentheses inside rounded and stadium labels and braces inside
diamond labels are unsupported. Edge labels are
plain text up to 256 bytes and may
be truncated visually; their full text is exposed to screen readers. The preview limits
graphs to 24 nodes and 48 edges after branch expansion and reports unsupported syntax instead of running
scripts or fetching content. Drag to pan and use the scroll wheel to zoom; arrow
keys pan, plus/minus zoom, and 0 resets to fit. Escape closes the preview. Layout
may contain overlapping edges, and other Mermaid diagram types are not supported.

Accessibility Test Runner checks the current `.html` or `.htm` buffer, including
unsaved changes, without executing code or making network requests. Findings can
be selected to navigate to their source. It checks a limited set of possible
missing HTML language, image alternative text and form-control labels, plus
repeated literal IDs that can make label and ARIA references ambiguous, possible
keyboard focus on elements marked aria-hidden, and literal viewport settings that
may restrict zoom below 200%. It flags empty document language and positive
`tabindex` values that may create an unexpected focus order. It also checks whether common ARIA ID references
resolve within fully parsed literal HTML, including forward references. Verify
these findings against the rendered page: ancestor state, CSS and scripts can
change focusability. It is a heuristic, not an accessibility conformance test.
CSS, contrast, keyboard use,
scripts, the rendered accessibility tree and dynamic markup are not evaluated.
IDs containing character references are not compared; missing-reference checks
are skipped if the parsed IDs or input are uncertain. Templates, scripts and
components can change IDs at runtime, so missing-reference findings need review.
Repeated IDs in templates may not reflect the final DOM. Truncated or uncertain markup is
reported as an incomplete audit.

SQL Workbench runs one selected SQL statement (or the whole open buffer) against a
project-relative standalone SQLite database. **Run read-only SQLite query** shows
bounded rows and **Explain SQLite query plan** shows the SQLite plan. The database
is copied into memory and cannot be modified by the query. Results and individual
cells can be truncated, and a running query can be cancelled with Escape. WAL-mode
databases are rejected; export a standalone snapshot first. Querying a file that is
changing concurrently may produce an inconsistent snapshot. From query results,
**Browse database schema** inspects the same in-memory snapshot used by that
query and lists user tables, views, indexes and triggers; selecting an object shows
its CREATE definition when available. For tables and views, it also lists column
types, nullability, default values, primary-key order and hidden or generated status.
Inspection is read-only, cancellable, and uses the retained query snapshot; truncated
object names are not used for column lookup. Large object lists or fields are marked truncated. The separate bound-query
commands accept a project-relative JSON file, for example
`{":name":{"type":"text","value":"Alice"},":id":{"type":"integer","value":"42"}}`.
Types are `null`, `integer` (decimal string), `real` (finite JSON number), `text`
(string), and `blob` (hex string). Named parameters or explicitly numbered
`?1` through `?64` parameters must be supplied exactly once. Numbered parameters
must be contiguous, and cannot be mixed with named or anonymous parameters;
unused bindings are rejected. Bindings are stored as plaintext in
the chosen file, so do not commit secrets. Results can be exported to a new
project-local CSV after confirmation; existing files are never overwritten. Partial
exports are identified explicitly. CSV contains plaintext results and may contain
secrets. Formula protection prefixes risky text and headers with an apostrophe,
changing those values. NULL and empty text may become indistinguishable in some
readers, and BLOBs use hex text that can be confused with ordinary text. Direct
writes to a live database, remote databases, and full-size BLOB viewing are not
supported. A separate change-copy workflow is described below.

Test Coverage Lens imports an existing project-local LCOV or Cobertura XML report. It lists covered
and uncovered lines, with uncovered lines first, and can open their source files.
**Browse files** shows per-file counts; select a file to filter the line list, then
choose **All files** to clear that filter. Unknown branch outcomes are shown separately.
The imported-line summary shows covered and uncovered counts and a percentage;
filters switch between all, covered, and uncovered lines or lines with known uncovered
or unknown branch outcomes while preserving navigation. Unknown outcomes are not
counted as uncovered.
The summary reflects only lines retained from the report, and labels incomplete imports.
**Compare with baseline** reads a second project-local report in the same format
and shows coverage changes by exact path and line number, with one-sided entries
separate from regressions. This is approximate: source edits may move or change
lines, and either report may be stale or incomplete. Branch counts show covered,
uncovered and unknown separately. Current-report entries can open the source;
baseline-only entries have no current location to navigate to.
LCOV `BRDA` records show per-line and total covered, uncovered and unrecorded
branch outcomes; repeated branch IDs are combined across records. Branches with
no matching line record or beyond the bounded import are omitted and mark the
report incomplete. A `-` count is shown separately as no count, not as a hit.
It flags malformed reports, inaccessible or outside-project files, and the 10,000-line
display limit as incomplete. Gutter markers and cursor status show imported coverage
for an unedited buffer that matches the source captured at import; markers disappear
when that buffer changes. The report can still predate the captured source.
Cobertura support covers class-level line hits, supported line-level branch
fractions and project-contained source paths. Method-level lines fill gaps where
there is no class-level record for that file and line; class-level records take
precedence when both are present. Method-level totals are not displayed.
Branch counts use `condition-coverage` fractions, accepting ordinary percentage
rounding. Conflicting duplicate counts are shown as unknown and mark the report
incomplete; invalid or missing branch data also marks it incomplete. When no
branch total is available, its unknown count cannot be quantified. If a line has
no branch fraction, uniquely numbered nested condition records with explicit
covered/total fractions are summed; percentages without totals cannot be inferred.
Ambiguous, missing or outside-project
paths are skipped and marked incomplete. External DTDs are never fetched, and
internal entities and unsupported XML constructs are rejected as incomplete. The
module does not run coverage tools itself.

Change Archaeology runs local Git commands to list up to 100 commits for the open
file and display a selected commit's diff. The list and diffs follow renames within
the open project; history stops at a rename across its boundary. Historical
lines are read-only and are not presented as locations in the current buffer.
To trace a function, select its lines and choose **Trace selected function or
lines**. Git follows the selected range and shows its historical patches. The
open buffer must exactly match the committed HEAD file, and the project must be
the repository root; history is limited to 100 commits and 2 MiB of Git output.
Range tracking can omit changes Git cannot trace, and historical content is
read-only. Git must be installed.

Benchmark Comparison reads two saved, project-local JSON reports on request. It
accepts Google Benchmark JSON or the simple `context`/`measurements` schema in
`src/modules/BenchmarkComparison.hpp`. Machine, build and environment context
must match before results can be classified; missing or mismatched context is
shown as incomparable. A change beyond 5% is a possible regression or improvement,
not a statistical conclusion. Filter to possible regressions or incomparable
measurements and select a result to inspect each side's unit, direction, statistic
and sample count. Unknown sample counts are labelled; no significance or noise
estimate is implied. Google Benchmark repetitions are averaged; sample ranges
and standard deviation are shown when available. Time units `ns`, `us`, `ms`
and `s` are converted for comparison while the original values and units remain
visible. Other mismatched units remain incomparable. Significance tests are not
performed, and the module does not run benchmarks.

Binary Inspector reads a project-local file of up to 128 MiB without executing it.
It shows architecture, sections, available symbols and linked libraries for ELF,
PE and Mach-O files, including universal Mach-O slices. For a universal file the
first slice is selected initially; choose Architectures to inspect another slice
from the same saved snapshot. A category overview shows counts and lets you browse
sections, libraries, and symbols separately; PE files also show imported and
exported names or ordinals, and export RVAs and forwarders when available. Escape
returns to the overview. Extended ELF section numbering is supported. For
sectionless ELF executables, libraries can be recovered from the dynamic segment
and mapped string table; section and symbol lists remain incomplete. Universal
Mach-O archive slices and nested universal files are not supported; metadata in
other slices is inspected when selected. Stripped binaries can lack symbols;
malformed or bounded metadata may be incomplete. Each category is capped at
2,000 entries.

Localization Workspace compares two saved, project-local JSON files up to 1 MiB
each. It lists missing and extra keys, compares Unicode code-point lengths, flags
text over 120 code points and characters in selected RTL script ranges, and shows
a plain-text preview. Open a comparison entry to navigate to its source or
translation value; navigation checks that the selected file still matches the
snapshot and asks you to resolve dirty open documents first. Objects and arrays are supported; arrays are compared by
position, and non-string scalar values are rejected. The preview does not simulate
RTL layout or shaping; code-point counts are not rendered width or grapheme counts.
It flags missing and extra simple `{identifier}` and positional `{0}` placeholders and differing counts
when both strings exist. This is not ICU message validation; escaped and complex
formats and formatting semantics may require manual review.
For source text that is not blank, the workspace also identifies an existing empty
or ASCII-whitespace-only translation as needing translation. Whitespace-only source
entries are not flagged. **Needs translation** filters to missing or blank entries;
this is a heuristic and does not treat non-ASCII space characters as blank.
For a source key, **Edit translation** accepts a single-line value up to the input
limit and shows a review with simple placeholder differences before confirmation.
An existing string can be replaced, or a missing leaf added to an existing object;
missing parent objects and array growth must be edited manually. Lucid rechecks
both compared files and open-document state, then opens the translation and applies
one undoable, unsaved document edit. Normal save conflict checks still apply. Long
or multiline translations can be edited directly in the JSON file.

Shader Workbench runs an explicitly selected `glslangValidator` against the saved
open GLSL shader after confirmation. Supported file extensions are `.vert`,
`.frag`, `.geom`, `.tesc`, `.tese`, and `.comp`. Parsed diagnostics in the open
file can be selected to navigate to their line; unparsed output is reported as
incomplete. The compiler can access files and is not sandboxed. Shader changes
invalidate the results. The optional SPIR-V action asks for a project-relative
output path and explicit confirmation. Existing output is replaced only if it
still matches the content reviewed before compilation; creation is exclusive.
Another process can still race the final file check. **Inspect includes** reads
saved project-local files without running the compiler and shows an advisory graph
of literal quoted includes. It does not evaluate conditions, macros or compiler
search paths; backslash-continued literal directives are followed, while unsupported, missing, cyclic and bounded results are marked
incomplete. Select a directive to navigate only while its source still matches
the captured bytes and has no unsaved edits. After saving a
SPIR-V artifact, **Inspect saved SPIR-V metadata** reads the current file and
shows entry points, stages and literal compute workgroup sizes when available.
This is a bounded metadata inspection, not shader validation, GPU compatibility
verification or a rendered preview; the saved artifact can change afterwards.
**Preview fragment shader** offers a separate, explicitly confirmed 256 × 256
GPU-rendered snapshot for a restrictive descriptor-free fragment interface.
The interface check is not complete SPIR-V validation. The shader executes on
the GPU and can hang or affect the driver; a supervised helper and timeout do
not sandbox GPU work. Use trusted shaders only. Other stages, resources, push
constants, built-ins and arbitrary uniforms are unsupported.

Design-Token Explorer reads a selected project-local CSS or JSON file up to 4 MiB.
It lists simple one-line CSS custom properties and primitive JSON or DTCG-style
values, with warnings for syntax it cannot interpret. Select a token to open its
definition, view static `var()` references in that CSS file, or search its exact name
across the project; navigation requires the file to match the
imported snapshot and open edits to be resolved. Aliases, transformed names and
CSS cascade are not resolved. JSON numbers retain their original text; token paths
and values are bounded, and excess warnings are labelled. Composite token values
and multiline CSS declarations are not supported. Reference detection handles
multiline `var()` calls but does not resolve the cascade, escaped names or comments
inside calls.

Log Correlation reads up to eight selected project-local logs, with a combined
16 MiB limit. It filters case-sensitively and aligns ISO timestamps with explicit
offsets, including timestamps in RFC 5424 version 1 syslog headers; traditional
syslog timestamps require a user-supplied year and fixed UTC offset. RFC 5424
headers are recognized for their timestamps rather than fully validated, and
fractional precision beyond milliseconds is truncated.
Indented lines attach to the preceding record. The results report unparsed lines
and scan limits, and can preview matching records. Select a record to open its
source line; navigation refuses changed snapshots, unsupported text and dirty
open documents. Syslog year rollover and
daylight-saving changes are not inferred. If the result cap is reached, the
earliest matching timestamps among scanned lines are retained; scan limits can
still omit records from later files.

Python Environments discovers candidate interpreters in the project, standard user
environment directories, Poetry's common and configured cache locations, pyenv's
versions directory, and PATH. Discovery does not execute Poetry or pyenv; it scans
bounded directory entries and skips symlinked version directories. Versions shown
during discovery are unverified.
Discovery is bounded and can omit interpreters; the remembered path remains visible
even when it is not found. **Choose interpreter path** accepts an absolute path
without executing it, so interpreters outside discovery roots can be selected.
The path is unverified and may no longer exist until explicitly validated.
Selecting **Validate and remember** runs the chosen interpreter and saves its path
as a per-user, per-project preference; **Create a virtual environment** validates
the interpreter, creates a new project-contained environment without pip, then
validates and remembers the result. The chosen executable can run arbitrary code
and may use the network. A failed creation can leave a partial directory, which
Lucid does not delete automatically. Python files use the remembered interpreter
to start `python -m pylsp` for supported language actions. Install
`python-lsp-server` in that environment; Lucid reports a stopped server if it is
missing. For a remembered interpreter, the module can install the pinned direct
`python-lsp-server` package from PyPI after a separate confirmation. It can use
existing pip or, only after choosing and confirming that option, run ensurepip
first. This executes the interpreter and downloaded packages, may change installed
dependencies, and may leave partial changes if cancelled or unsuccessful. The
direct package is pinned; pip resolves its dependencies. Switching interpreters
restarts Python language intelligence on the next request. C and C++ continue to
use clangd.

Test Runner Plus discovers tests from a project-local CMake build directory using
a reviewed CTest executable. On supported systems it can run one explicitly
selected test and show its failure output and previous status. Up to 10 runs per
project and build are kept locally with test names, status and duration only; test
output and failure text are not stored. Paths and test names can still be sensitive.
The selected test shows saved pass/fail/skip counts, consecutive and longest
observed failure streaks, the saved run containing its latest failure, pass/fail
transitions, and a duration difference when two recorded durations are available.
Transitions can help identify tests to investigate but do not prove flakiness.
Runs that do not include that test are not counted; skipped and ambiguous results
break transition sequences, and duplicate names are marked ambiguous.
**Review recorded failures** lists the latest retained observations, including
entries that cannot safely be matched to a current test. This is not a complete
inventory of failures. Choose a runnable entry and confirm its individual rerun;
Lucid rediscovers the test and verifies its exact name before running it. Escape
requests cancellation, and previously saved results remain available.
CTest discovery and tests can execute project code with the user's privileges,
including network access. Automatic fixture setup and cleanup tests are excluded to
avoid running unselected tests; this can change a fixture-dependent test's behavior.
Escape requests cancellation of discovery or a selected run. Windows execution is
implemented but has not yet been verified on Windows. Common parameterized names
with `[]`, parentheses and regex metacharacters are matched literally; some other test-name characters and
builds outside the open project are not yet supported.

Hex Editor opens a project-local file up to 16 MiB in a paged hexadecimal view.
Selections of 1, 2, 4 or 8 bytes show their unsigned and two's-complement signed
integer values in little- and big-endian order; interpretation does not edit bytes.
Choose a row or offset to overwrite or insert bytes, select and delete a range,
search byte sequences, and undo or redo edits. Find next and previous continue the
last search, select each matching range, include overlapping matches, and wrap at
the file boundaries. Editing, undoing or redoing clears the remembered search.
Saving is explicit and checks that
the on-disk bytes still match the opened version; a conflicting external change is
reported and the edits stay in memory. Input is limited to 4,096 bytes per edit;
large deletions that exceed the undo-history limit are refused.

Embedded C/C++ can inspect a saved project-local GNU ld map up to 8 MiB. It shows
regions and output sections, with separate estimated virtual-address (VMA) usage
and explicitly reported load-address (LMA) usage. Do not add the two estimates:
load addresses omitted by the map remain unknown, and padding, load images or
unsupported constructs may be missing. **Inspect toolchain profiles** reads a
project-local JSON file such as `{"profiles":[{"name":"Board","target":"arm-none-eabi","cCompiler":"/opt/toolchain/bin/cc"}]}`.
Optional `cppCompiler`, `toolchainFile` and `sysroot` paths must be absolute.
These are unverified declarations: Lucid does not inspect or run the tools or
configure a build. Targets can be added, replaced or removed from the profile JSON:
enter one profile as `{"profiles":[{...}]}`, review the change and explicitly
confirm the save. Saving normalizes JSON formatting, requires the profile file to
be closed, and refuses an external change since inspection; no compiler runs.
A selected profile can also configure an existing project-local build directory:
choose an absolute CMake executable, review its exact arguments and explicitly
confirm. CMake and project scripts run with your privileges and can change files;
cancellation can leave build files changed. The declared target triple is shown but
not automatically applied as a compiler flag. Other project files and the executable
may change after review, and existing CMake cache settings can conflict. **Inspect SVD registers** reads a project-local CMSIS-SVD file
up to 4 MiB and shows declared peripheral/register addresses, explicit reset values,
sizes, bit fields, bounded descriptions and declared numeric field enumeration
choices (including usage and defaults when supplied). Choices are unverified metadata,
not live register values; wildcard/binary-pattern encodings and derived enumeration
groups are not expanded. It does not access hardware. Its
conservative XML subset accepts predefined and numeric text entities but rejects
DTDs and custom or external entities. It does not expand forward or qualified references, derived peripheral arrays or nested and derived clusters. A field can inherit from an earlier literal field in the same register, including its bit range, access, description and enumerations; supported explicit properties override them, and explicit enumeration groups replace inherited choices. An `lsb`/`msb` override must provide both, and mixed bit-range forms are rejected. Single-level clusters, including simple
arrays with explicit increments and numeric or comma-separated indexes, expand
register names and addresses. Cluster size, access and reset values override peripheral and device defaults for contained registers when declared before them; register values take precedence. Cluster reset masks and protection defaults are not inherited and are marked incomplete. A peripheral can inherit from a unique earlier complete peripheral when it has an explicit name and base address and no register or default-property overrides; copied register addresses are relocated. Unsupported or ambiguous inheritance is marked incomplete. A register can inherit from a unique earlier register in the
same peripheral, with complete explicit field overrides. Simple peripheral arrays expand up to 256 elements with one `%s` placeholder, a nonzero address increment and numeric or unique comma-separated literal indexes; array properties must precede registers. Simple register arrays with explicit increments
and numeric or comma-separated indexes are expanded. Field arrays expand up to
64 elements with a nonzero bit increment, one `%s` name placeholder and numeric
or comma-separated literal indexes; bit ranges and expanded metadata are bounded.
Array `dimIndex` also accepts one inclusive decimal or same-case ASCII-letter range with exactly `dim` entries; zero-padded decimal endpoints must have equal width. Comma-separated indexes are treated as literals, so mixed ranges and lists are not expanded. Register arrays inside cluster arrays are expanded independently, including within peripheral arrays, subject to the same output, metadata and address limits. Omitted data is marked incomplete with a reason. Device and peripheral size,
access and reset defaults are inherited when declared before registers, and
register access is inherited by fields. Other linker-map formats
and active target management are not supported.

Crash Dump Reader inspects a project-local minidump up to 128 MiB without uploading
it. It shows available exception, thread, architecture and loaded-module metadata.
For supported x86-64 contexts it also shows the initial program counter and
frame-pointer candidates. These candidates are heuristic, may be wrong or
incomplete, and are not a reliable stack unwind. The display is limited to 512
threads, modules and frame candidates each. Minidumps can contain sensitive data.
An optional project-local TSV source map can associate exact module names and
module-relative half-open address ranges with project source lines. Each row is
`module<TAB>start RVA in hex<TAB>end RVA in hex<TAB>relative path<TAB>line<TAB>symbol`.
Choose Load source map and select a mapped frame to navigate. The same picker accepts
project-local Breakpad `.sym` files with project-relative FILE paths and FUNC line
records. For Windows RSDS debug records, Lucid compares the module name, CPU
architecture and debug ID, and rejects confirmed mismatches. A metadata match does
not authenticate the symbol file or verify source content; unavailable identifiers
remain unverified. Absolute FILE paths and PUBLIC-only symbols are not mapped. Mappings and frame
candidates are unverified and may be wrong or stale; ambiguous matches are not
navigable. Source must be inside the project, readable as text and contain the
specified line; resolve unsaved edits before navigation. No PDB/DWARF parsing,
automatic symbol download or direct core dump parsing are supported. Saved GDB `thread apply all bt` text is also accepted up to 4 MiB; it displays reported signals, thread headings and frame text as unverified, with no source navigation or symbolication. The supported text subset is bounded to 256 threads, 4096 frames and 4096 bytes per line; only 512 frames are displayed. Unsupported lines and truncation are marked incomplete. Saved LLDB `thread backtrace all` text is also accepted within those limits, showing reported threads and frames as unverified, without navigation or symbolication. Unsupported LLDB records are marked incomplete. Legacy Apple crash report text is also accepted with the same limits; reported code type, exception, threads and frames are unverified and cannot be navigated. Unsupported sections are marked incomplete. Current JSON `.ips` reports are accepted for a bounded subset of exception, image and thread/frame metadata. The reader omits image paths and process details, but the input, image names and symbols can contain sensitive information. Addresses and symbols are unverified. An explicitly selected project-local TSV map can offer source navigation for a complete report with an unambiguous image name and non-overlapping matching range; the map and source remain unverified, and the same project and unsaved-edit checks apply. UUID identity is not checked, and Breakpad maps are not supported for IPS. Unsupported variants, thread state, symbolication and other debugger text formats are not supported.

Rust Ownership Lens reads saved rustc or Cargo newline-delimited JSON diagnostics
from the project. It presents supported ownership and borrowing errors, compiler
notes, explanations, spans and suggestions. Eligible single-span
`MachineApplicable` suggestions can be reviewed with their original and replacement text and applied as an undoable,
unsaved edit after their compiler source lines and the saved file are checked.
Multipart fixes, missing source excerpts, text over 2048 bytes, and unsupported control characters cannot
be applied this way; matching excerpts do not prove other project files are current. Source
navigation is limited to Rust files inside the project. Primary, related and note
spans are labelled; all compiler-provided source lines are checked before opening,
and dirty open documents must be resolved first. Spans without source text are
labelled unverified; even matching excerpts cannot prove the entire file is current. The
module does not infer ownership independently. **Run Cargo ownership check** is a
separate action: choose an absolute Cargo executable, review the exact `check --locked
--offline --message-format=json` arguments, and explicitly confirm. Cargo, build
scripts and procedural macros run with your privileges; `--offline` does not prevent
those scripts or child processes from accessing the network. Output and runtime are
bounded, cancellation may leave changed files, and partial/interleaved diagnostics
are marked incomplete. Save open edits first. The runner inherits Lucid's working
directory; other source files, lockfile and executable may change after review.

Dependency Risk Review parses the open, possibly unsaved npm `package-lock.json`
(v1/v2/v3), Go `go.sum`, or Cargo `Cargo.lock` (v3/v4) locally. npm v1 nested
entries are checked but direct dependencies cannot be inferred from that lockfile
alone. After the user confirms, it sends eligible package
names, ecosystems and exact versions to `api.osv.dev` and shows returned advisory IDs. Selecting an ID
offers a separate confirmation to fetch its summary, aliases, references and reported
npm, Go or crates.io fixed versions; verify the affected range before upgrading. Escape
cancels; skipped aliases, noncanonical sources and unresolved packages are
reported as coverage limits. For Go, `go.sum` is checksum history, not the selected
build graph: findings can include unused versions, and local replacements, missing
checksums and go.mod-only entries are not assessed. Cargo review covers exact
checksummed crates.io packages only; workspace, Git and alternative registry entries
are skipped, and direct dependencies and enabled features or targets are not inferred.
Unsupported multiline TOML is rejected. OSV data may be incomplete and
findings do not establish exploitability or reachability. Other ecosystems and full
dependency provenance are not yet supported.
**View declared package provenance** shows each eligible package's lockfile source
and supported checksum without a network request. These values are declarations;
the provenance view does not download packages or verify their contents. A Go checksum alone does
not establish its source, and unavailable or unsupported checksums are labelled.
The list is limited to 1,000 displayed packages; OSV queries send only ecosystem,
package name and version, not source URLs or checksums.
For an npm SHA-256, SHA-384 or SHA-512 SRI, crates.io SHA-256, or Go h1 entry,
**Verify a local artifact**
asks for a project-relative file and explicit confirmation before reading up to
64 MiB locally. The lockfile buffer, disk baseline, project and module are rechecked
around the cancellable hash operation. A match concerns the bytes read against the
lockfile checksum; it does not verify the publisher, source, lockfile trust, or that
the file remains unchanged afterward. Another process can change the path or file
between containment checks and reading. Go verification hashes module ZIP entry
names and uncompressed content, not the ZIP file's raw bytes. Only stored or deflate
ZIP entries are supported; ZIP64, more than 10,000 entries, entries over 16 MiB,
and total expanded content over 128 MiB are rejected. No archive is extracted.
Microcontroller Dashboard declares device access separately. It lists serial
ports by operating-system name; this does not identify boards. Selecting a port
presents an explicit choice to capture for five seconds at 9600 or 115200 baud.
Opening or configuring a port can change DTR/RTS, reset hardware, or affect
another process. Capture is bounded and cancellable, with best-effort settings
restoration. Continuous monitoring at 115200 baud requires a separate confirmation,
opens the port once, stops on request or after 24 hours, and retains the latest 1 MiB
with an overwritten-byte notice. Binary output is not displayed as text. Stopping
requests cancellation and waits for the worker to finish restoring settings. Binary data is not shown as text. Its OpenOCD profile inspector
accepts a restricted declarative subset without executing it. To flash, open a
supported profile naming a project-local ELF or HEX firmware file, choose an
OpenOCD executable, and review the firmware snapshot SHA-256, interface and
target before confirming. Confirmation runs that executable and its configuration
scripts with your privileges and can erase the connected device. Failure or
cancellation can leave hardware partially programmed; verify the device after
the operation. Raw BIN images require an explicitly entered hexadecimal flash address;
the review shows the address range, byte count and SHA-256. A wrong address can
destroy data, and erase operations may extend beyond the displayed range. No target
memory-map verification is performed. **Identify USB boards** accepts project-local JSON such as
`{"boards":[{"name":"My board","vendorId":"1234","productId":"abcd","profile":"boards/target.cfg"}]}`.
After explicit confirmation it reads USB VID/PID descriptors via Linux sysfs,
macOS IOKit or Windows SetupAPI without opening ports. Matches are project declarations,
not proof of a board: an ID can identify a shared probe. Duplicate mappings are
shown as ambiguous, and discovery never flashes hardware or opens the listed profile.
Windows discovery has been cross-compiled but not tested at runtime; physical-device
discovery needs further testing on all platforms.

SQL Workbench can preview a single INSERT, UPDATE, DELETE or REPLACE on an isolated
standalone SQLite snapshot and, after reviewing the statement and affected-row count,
create a separate new `.db` file. It never overwrites the source. WAL databases,
SQLite sidecars, triggers, parameters, RETURNING, schema changes, and multiple
statements are rejected. The preview lists up to 200 observed table, operation and
row ID events without cell values; row IDs can be sensitive. This is an incomplete
list, not a row-by-row diff: SQLite may omit WITHOUT ROWID table changes,
truncate-optimized whole-table deletions and REPLACE conflict deletions. Source bytes
are checked again before creation, but external processes are not locked and may
change files or paths between checks; close the database and use a standalone copy.

API Request Studio can check a received response against a project-local assertion
JSON file, without sending another request. For example:

```json
{"assertions":[{"type":"status","value":200},{"type":"body_contains","value":"ready"},{"type":"json_string_equals","pointer":"/data/id","value":"123"},{"type":"json_bool_equals","pointer":"/ready","value":true},{"type":"json_null","pointer":"/error","value":null}]}
```

Checks show pass/fail without displaying expected values, JSON paths or response
contents. Assertion files may themselves contain sensitive plaintext. JSON checks
parse at most 1 MiB of the response, support string, boolean and exact decimal number equality and null checks, and fail on
ambiguous duplicate keys along a pointer; unsuccessful or incomplete responses
cannot be checked. These checks do not establish that a service is secure.
For example, `{"type":"json_number_equals","pointer":"/count","value":123}`
compares decimal values without rounding through floating point. Number tokens
are limited to 4096 bytes and exponents to an absolute value of 1,000,000;
out-of-range response values do not match.
An assertion such as `{"type":"header_equals","name":"Content-Type","value":"application/json"}`
checks a single header in the final complete HTTP response block. Header names
are case-insensitive and surrounding HTTP whitespace is ignored in the received
value. Duplicate headers, malformed blocks and incomplete headers do not match;
values and names are not exposed in the result.

The bound change-copy command also accepts a project-local typed bindings JSON
file in the same format as bound queries. It binds simple `:name`, `@name` and
`$name` and contiguous numbered `?1` values without SQL interpolation. Mixing
named and numbered parameters, anonymous parameters, and SQLite's extended
`$name::suffix(...)` forms are unsupported.
Values are hidden in the preview and may be sensitive plaintext in the bindings
file. Before creating the copy, Lucid checks that the saved binding bytes still
match the preview and rejects detected unsaved editor changes. External writers
are not locked, so filesystem changes can still race these checks.

Select an observed SQL row to inspect its before-and-after snapshot values.
Values may contain secrets and appear only after selection; blobs are rendered as
bounded hexadecimal and long values are marked truncated. This is a net view of
one row ID, not a history: row ID changes and REPLACE may look like insertion or
deletion, and SQLite can omit operations from the observed list. Equal displayed
numeric text does not prove exact binary equality. Closing the view clears the
shown values from the UI.

Visual Regression Review can inspect a 512×512 source-pixel crop of the
baseline, candidate and difference snapshots. Arrow keys move the crop by 256
source pixels; 0 returns to the overview and Escape closes the preview. The crop
contains one output pixel per source pixel before display; the window may scale
it. Transparency uses the overview checkerboard, and the reviewed threshold and
masks still apply to the difference view.

For a configured visual review, open its configuration document to add or remove
masks or change the threshold. Lucid previews the old and new changed, masked,
and compared pixel counts before applying one undoable document edit. The JSON
is normalized; the document must still be saved separately. The reviewed
screenshots and document are rechecked before applying, although external
processes can still race filesystem checks.

Log Correlation offers a separate JSON Lines command for mixed structured and
plain logs. It recognizes one top-level string field named `timestamp`,
`@timestamp`, or `time` containing an ISO timestamp with an explicit timezone;
ambiguous fields, numeric or zone-less timestamps, and JSON lines over 64 KiB
are counted as unparsed. Filtering searches the raw source line and source name,
not decoded JSON fields. Log records may contain secrets and are not redacted.

Benchmark Comparison accepts custom measurements with a `samples` array in
place of `value`, and retains repeated Google Benchmark measurements. When raw
repetitions are available it shows their count, minimum, maximum and sample
standard deviation. Aggregate-only means and single values do not provide a
spread. The displayed variability and 5% threshold are descriptive; they do
not test statistical significance or establish a regression.

Design-Token Explorer resolves whole-value JSON/DTCG aliases such as
`{color.primary}` within the selected saved file. It identifies missing targets,
cycles, ambiguous names and chains exceeding 64 hops; following a target checks
that the imported source snapshot still matches. Embedded expressions, composite
values, JSON pointers, cross-file references, CSS cascade and type compatibility
are not resolved. Imported token values may themselves be sensitive.

Configured visual reviews also offer **Draw mask on candidate**: drag inside the
candidate image, inspect the outline, then press Enter to review the source-pixel
rectangle and its effect on coverage counts. Escape cancels. Dragging outside the
image is rejected; manual coordinate entry remains available from the keyboard.
Applying the draft changes only the open configuration document and remains
undoable, with the same screenshot and document checks as other config edits.

Test Runner Plus can import an existing project-local JUnit XML report without
running tests. It displays a read-only snapshot and can compare a second saved
snapshot by exact class and test name. Duplicate identities are ambiguous, and
one-sided tests may come from partial runs. Imported output may contain secrets;
report snapshots may be stale. JUnit formats vary; unknown elements, unsupported testcase attributes and invalid
hierarchies are rejected so unrecognized failure states are not shown as passes.
No commands, test reruns or source locations are inferred from the report.
Representative generated fixtures cover pytest 8.4 xUnit2 and legacy, Jest 29.7
with jest-junit 16, and JUnit Platform 1.11.4. Repeated same-kind failures are
retained. Suite totals must agree with emitted rows or distinct class/name
identities; the latter accommodates pytest call-plus-teardown rows, which remain
ambiguous. Other producer versions and dialect options are not guaranteed.

Before preparing an environment-backed API request, **Check** runs an optional
local template preflight. It lists referenced and missing variable names,
malformed placeholders, secret-marked values referenced in URLs and unused
profile variables without showing values or sending a request. Names can be
sensitive. If placeholders are malformed the unused list is withheld. The
check uses the imported saved environment snapshot and may become stale; it
does not validate destination safety or replace send-time checks and consent.
