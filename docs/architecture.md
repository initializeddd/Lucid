# Architecture

Lucid is a C++20 desktop editor with a GLFW window and a Vulkan renderer. The application coordinates the document, project, services and input; the renderer consumes a frame state rather than owning project files. macOS uses MoltenVK.

## Application and editor

`src/app/Application` routes keyboard, mouse and native menu events, manages the open project and documents, and builds `ui::FrameState` for rendering. The document model stores Unicode text, cursor and selection state, undo/redo and serialization information. A temporary switcher keeps inactive documents and their editing context without permanent tabs.

File writes check for external modifications, preserve supported metadata and create local history before replacing an existing file. Recovery snapshots are periodic and separate from clean-exit session records. These protections have limits: see [Sessions and recovery](../README.md#sessions-and-recovery) and [Recovery snapshots](../README.md#recovery-snapshots).

## Rendering and interaction

`src/renderer/vulkan` draws text, shapes, editor chrome and temporary command surfaces. The background scene renderer draws animated 3D scenes; a separable Gaussian blur softens them when a document is open. Scene choice, primary color, motion, zoom and animation speed are controlled by settings. The welcome content, shortcut list, command suggestions and status information use time-based transitions.

`src/ui` contains presentation state, layouts and theme data. The command surface exposes project and file selection, settings and other workflows. Some lists use pages, while file and project suggestions scroll; animated edge fades indicate additional content. macOS has an initial accessibility bridge for editor text and visible command options. Other screen-reader support and complete accessible editing remain work in progress.

## Workspace services

`src/workspace` contains project indexing, project-wide search and reviewed replacement, Git operations, tasks, recovery and session persistence. Index and function scans run asynchronously. Search and replace use bounded results and check for changed files; multi-file replacement is not an atomic undo transaction.

C/C++ comment blocks define compile functions executed by the platform process layer. Output appears in a selectable compile window. The LLDB debug-adapter client supplies breakpoint and stepping workflows; platform coverage and functionality remain limited as described in the [README](../README.md#debugging).

The language-server client starts an installed `clangd` for C/C++ completion, diagnostics, navigation, references and formatting. Local fallback completion remains available without it. See [Language intelligence](../README.md#language-intelligence) for the current scope.

## AI, modules and collaboration

`src/ai` provides an in-process local inference integration, verified model download and optional OpenRouter requests. The user chooses a provider per workflow and reviews generated edits. Remote requests transmit the relevant context to the selected provider.

Modules currently provide declarative text insertion only; requested privileged capabilities cannot be enabled. Collaboration code is a foundation rather than a supported editor feature. See the [module guide](modules.md) and [collaboration status](collaboration-design.md).

## Packaging and validation

CMake builds the app, shaders and tests. GitHub Actions builds macOS, Linux, Windows, FreeBSD and OpenBSD release artifacts. macOS and Windows distribution signatures are not provided. BSD builds use system libraries and have limited graphical validation. See [Building](building.md), the [README](../README.md#downloads) and [changelog](../CHANGELOG.md) for requirements and release status.
