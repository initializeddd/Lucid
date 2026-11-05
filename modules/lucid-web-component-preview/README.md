# Web Component Preview: implementation status

This module is not yet installable as a working preview. The current host-side
foundation validates project-local HTML configurations and provides an in-memory
asset router for an explicit snapshot of selected HTML, JavaScript, CSS, image,
and font files. The router performs no file or network access. It restricts paths,
methods, file types and snapshot size, and supplies restrictive response headers.
A preview host must build snapshots using the project's file access controls and
rebuild them when files change.

There is no browser engine, HTTP server, screenshot capture, viewport rendering,
or component-state UI yet. The router's Content Security Policy is defense in
depth, **not a security sandbox**: JavaScript can still navigate a page or exploit
browser behavior. Before executing project code the host needs proper isolation,
explicit trust and permission controls, request-origin validation and a browser
lifecycle; the snapshot router alone must not be exposed on a network interface or
represented as making untrusted HTML safe. Inline scripts, SVG and many resource
types are currently unsupported by this router.
