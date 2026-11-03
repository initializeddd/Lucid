# Lucid modules (preview)

Lucid currently supports **data-only text commands**. They can insert a fixed text
string into the open editor when you choose the command. They cannot execute code,
read project files, access the network, launch processes, or read or change settings.
A module declaring any of those four permissions cannot be enabled in this version.
There is no arbitrary-code module runtime or package marketplace yet.

Create a folder under Lucid's `Modules` directory, named with lowercase ASCII
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
