# Lucid modules (preview)

Lucid currently supports **data-only text commands**. They can insert a fixed text
string into the open editor when you choose the command. They cannot execute code,
read project files, access the network, launch processes, or read or change settings.
A module declaring any of those four permissions cannot be enabled in this version.
There is no arbitrary-code module runtime. The GitHub catalog lists modules
published in Lucid's catalog package; it is not a global index of every GitHub
package. The catalog and starter package are public and can be downloaded
without a GitHub account.

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
generates the catalog from those manifests. New GitHub packages must be made
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
Removal deletes the entire module folder after confirmation. Manifest changes are
rechecked before commands run. To change a manifest, disable the module first and
then reopen the manager.

A manifest is limited to 64 KiB and 32 commands; command text is limited to 4096
bytes. Permission requests are not grants: modules requesting unsupported access
stay disabled. This format is not compatible with VS Code extensions.
