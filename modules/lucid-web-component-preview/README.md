# Web Component Preview

On macOS, Lucid can capture a reduced preview of explicitly selected project-local
HTML, JavaScript, CSS, image and font assets. Open a preview configuration and
select **Render web preview**. The preview is a point-in-time snapshot; it is not
interactive and asynchronous page work may not have finished when it is captured.
See `docs/modules.md` for the configuration format and platform limitations.

Assets are copied into a bounded in-memory router, which does not read the file
system or make network requests while serving them. SVG responses have a separate
restrictive policy intended to block scripts and external resources. These controls
are defense in depth, **not a security sandbox**: use only trusted project code.
The preview can execute JavaScript in WebKit and network isolation is not guaranteed.
Windows and Linux previews remain unavailable pending runtime verification.
