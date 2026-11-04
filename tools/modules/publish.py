#!/usr/bin/env python3
"""Publish checked-in declarative modules and generate their GHCR catalog."""
import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULES = ROOT / "modules"
ID = re.compile(r"[a-z0-9][a-z0-9_-]{0,79}\Z")
VERSION = re.compile(r"[a-z0-9][a-z0-9._-]{0,159}\Z")


def collect(owner):
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]{0,38}", owner):
        raise ValueError("Invalid GitHub package owner")
    entries = []
    for directory in sorted(MODULES.iterdir()):
        if directory.is_symlink() or not directory.is_dir():
            continue
        module_id = directory.name
        if not ID.fullmatch(module_id):
            raise ValueError(f"Invalid module ID: {module_id}")
        path = directory / "module.json"
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 65536:
            raise ValueError(f"Missing or unsafe manifest: {path}")
        manifest = json.loads(path.read_text(encoding="utf-8"))
        name, version = manifest.get("name"), manifest.get("version")
        description = manifest.get("description", "")
        if not isinstance(name, str) or not 1 <= len(name.encode()) <= 128 or not isinstance(version, str) or not VERSION.fullmatch(version):
            raise ValueError(f"Invalid name or version: {path}")
        if not isinstance(description, str) or len(description.encode()) > 512:
            raise ValueError(f"Invalid description: {path}")
        permissions, commands = manifest.get("permissions"), manifest.get("commands")
        if not isinstance(permissions, dict) or set(permissions) != {"project", "network", "processes", "settings"} or any(value is not False for value in permissions.values()):
            raise ValueError(f"Unsupported permissions: {path}")
        if not isinstance(commands, list) or not 1 <= len(commands) <= 32:
            raise ValueError(f"Invalid commands: {path}")
        for command in commands:
            if not isinstance(command, dict) or any(not isinstance(command.get(key), str) or not 1 <= len(command[key].encode()) <= limit for key, limit in (("title", 128), ("insert", 4096))):
                raise ValueError(f"Invalid command: {path}")
        suffix = module_id.removeprefix("lucid-")
        package = f"{owner}/lucid-module-{suffix}"
        entry = {"id": module_id, "name": name, "description": description, "package": package, "version": version}
        if any(existing["package"] == package for existing in entries):
            raise ValueError(f"Duplicate package name: {package}")
        entries.append(entry)
    if not entries:
        raise ValueError("No modules found")
    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--owner", default="initializeddd")
    parser.add_argument("--publish", action="store_true")
    args = parser.parse_args()
    entries = collect(args.owner)
    catalog = {"modules": entries}
    if not args.publish:
        print(json.dumps(catalog, indent=2, ensure_ascii=False))
        return
    for entry in entries:
        source = f"modules/{entry['id']}/module.json:application/vnd.lucid.module.v1+json"
        subprocess.run(["oras", "push", f"ghcr.io/{entry['package']}:{entry['version']}", "--artifact-type", "application/vnd.lucid.module.v1", source], cwd=ROOT, check=True)
    (MODULES / "catalog.json").write_text(json.dumps(catalog, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    subprocess.run(["oras", "push", f"ghcr.io/{args.owner}/lucid-module-catalog:latest", "--artifact-type", "application/vnd.lucid.catalog.v1", "modules/catalog.json:application/vnd.lucid.catalog.v1+json"], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
