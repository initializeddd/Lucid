#!/usr/bin/env python3
"""Publish checked-in declarative modules and generate their GHCR catalog."""
import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MODULES = ROOT / "modules"
ID = re.compile(r"[a-z0-9](?:[a-z0-9_-]{0,78}[a-z0-9])?\Z")
VERSION = re.compile(r"[a-z0-9](?:[a-z0-9._-]{0,158}[a-z0-9])?\Z")


def module_version_exists(reference, expected):
    """Check all module tags before writing; errors other than explicit 404 fail closed."""
    try:
        raw = subprocess.check_output(["oras", "manifest", "fetch", reference], cwd=ROOT,
                                      stderr=subprocess.PIPE)
    except subprocess.CalledProcessError as error:
        detail = (error.stderr or b"").decode("utf-8", "replace")
        if re.search(r"\b404\s+Not Found\b", detail, re.IGNORECASE):
            return False
        raise ValueError(f"Cannot establish whether module version exists: {reference}") from error
    manifest = strict_json(raw)
    layers = manifest.get("layers") if isinstance(manifest, dict) else None
    digest = "sha256:" + hashlib.sha256(expected).hexdigest()
    if (not isinstance(layers, list) or len(layers) != 1 or not isinstance(layers[0], dict) or
            layers[0].get("mediaType") != "application/vnd.lucid.module.v1+json" or
            layers[0].get("digest") != digest or layers[0].get("size") != len(expected)):
        raise ValueError(f"Module version already exists with different content: {reference}")
    return True


def strict_json(text):
    """Reject ambiguous objects and non-JSON numeric constants at every depth."""
    def object_pairs(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError("Duplicate JSON key")
            result[key] = value
        return result

    def invalid_constant(value):
        raise ValueError("Invalid JSON numeric constant")

    return json.loads(text, object_pairs_hook=object_pairs, parse_constant=invalid_constant)


def collect(owner):
    if not re.fullmatch(r"[a-z0-9](?:[a-z0-9-]{0,37}[a-z0-9])?", owner):
        raise ValueError("Invalid GitHub package owner")
    entries = []
    for directory in sorted(MODULES.iterdir()):
        if directory.is_symlink():
            raise ValueError(f"Unsafe symlink in modules directory: {directory}")
        if not directory.is_dir():
            continue
        module_id = directory.name
        if not ID.fullmatch(module_id):
            raise ValueError(f"Invalid module ID: {module_id}")
        path = directory / "module.json"
        if path.is_symlink() or not path.is_file() or path.stat().st_size > 65536:
            raise ValueError(f"Missing or unsafe manifest: {path}")
        manifest = strict_json(path.read_text(encoding="utf-8"))
        if not isinstance(manifest, dict):
            raise ValueError(f"Invalid module manifest: {path}")
        name, version = manifest.get("name"), manifest.get("version")
        description = manifest.get("description", "")
        if not isinstance(name, str) or not 1 <= len(name.encode()) <= 128 or not isinstance(version, str) or len(version.encode()) > 64 or not VERSION.fullmatch(version) or ".." in version:
            raise ValueError(f"Invalid name or version: {path}")
        if not isinstance(description, str) or len(description.encode()) > 512:
            raise ValueError(f"Invalid description: {path}")
        requirements = manifest.get("requires", [])
        if not isinstance(requirements, list) or len(requirements) > 32 or any(not isinstance(item, str) or not 1 <= len(item.encode()) <= 80 for item in requirements) or len(set(requirements)) != len(requirements):
            raise ValueError(f"Invalid capability requirements: {path}")
        if "snippet-linked-v1" in requirements and "snippet-placeholders-v2" not in requirements:
            raise ValueError(f"Linked fields require multiple snippet placeholders: {path}")
        permissions, commands = manifest.get("permissions"), manifest.get("commands")
        if not isinstance(permissions, dict) or set(permissions) not in ({"project", "network", "processes", "settings"}, {"project", "network", "processes", "settings", "devices"}) or ("devices" in permissions and not isinstance(permissions["devices"], bool)) or not isinstance(permissions["project"], bool) or not isinstance(permissions["processes"], bool) or not isinstance(permissions["settings"], bool) or not isinstance(permissions["network"], bool):
            raise ValueError(f"Unsupported permissions: {path}")
        if not isinstance(commands, list) or not 1 <= len(commands) <= 32:
            raise ValueError(f"Invalid commands: {path}")
        for command in commands:
            if not isinstance(command, dict) or not isinstance(command.get("title"), str) or not 1 <= len(command["title"].encode()) <= 128:
                raise ValueError(f"Invalid command: {path}")
            if "action" in command:
                if not ((command["action"] in ("scan-secrets", "browse-cmake", "prepare-cmake-query", "markdown-local-links", "coverage-lcov") and permissions["project"]) or (command["action"] in ("serial-monitor", "board-profile") and permissions.get("devices") is True and "microcontroller-serial-v1" in requirements) or (command["action"] == "markdown-external-links" and permissions["project"] and permissions["network"] and "markdown-external-links-v1" in requirements) or (command["action"] == "web-preview" and permissions["project"] and permissions["network"] and "web-component-preview-macos-v1" in requirements) or (command["action"] == "flash-firmware" and permissions.get("devices") is True and permissions["project"] and permissions["processes"] and "openocd-flash-v1" in requirements) or (command["action"] == "compile-shader" and permissions["project"] and permissions["processes"] and "shader-compile-v1" in requirements) or (command["action"] == "fetch-pull-request" and not permissions["project"] and permissions["network"] and "pull-request-companion-v1" in requirements) or (command["action"] == "configured-container-actions" and permissions["project"] and permissions["network"] and permissions["processes"] and "container-project-actions-v1" in requirements) or (command["action"] == "inspect-containers" and (not permissions["project"] or "container-project-actions-v1" in requirements) and permissions["processes"] and permissions["network"] and "container-inspection-v1" in requirements) or ((command["action"] == "compare-visual-images" and permissions["project"] and "visual-regression-review-v1" in requirements) or (command["action"] == "compare-visual-configured" and permissions["project"] and "visual-regression-config-v1" in requirements)) or (command["action"] == "review-dependency-risk" and (not permissions["project"] or "dependency-artifact-verify-v1" in requirements) and permissions["network"] and "dependency-risk-review-v1" in requirements) or (command["action"] == "identify-usb-boards" and permissions["project"] and permissions.get("devices") is True and "usb-board-identification-v1" in requirements) or (command["action"] == "inspect-svd-registers" and permissions["project"] and "svd-registers-v1" in requirements) or (command["action"] == "inspect-toolchain-profiles" and permissions["project"] and "toolchain-profiles-v1" in requirements) or (command["action"] == "edit-cmake-options" and permissions["project"] and "cmake-cache-edit-v1" in requirements) or (command["action"] == "coverage-cobertura" and permissions["project"] and "coverage-cobertura-v1" in requirements) or (command["action"] == "open-api-environment" and permissions["project"] and permissions["network"] and "api-environments-v1" in requirements) or (command["action"] == "edit-api-collection" and "api-collection-edit-v1" in requirements) or (command["action"] == "open-api-requests" and permissions["network"] and "api-request-studio-v1" in requirements) or (command["action"] == "sql-change-bound" and permissions["project"] and "sql-change-bound-v1" in requirements) or (command["action"] == "sql-change-preview" and permissions["project"] and "sql-change-copy-v1" in requirements) or (command["action"] in ("sql-query-bound", "sql-explain-bound") and permissions["project"] and "sql-parameters-v1" in requirements) or (command["action"] in ("sql-query", "sql-explain") and permissions["project"] and "sql-snapshot-v1" in requirements) or (command["action"] == "release-note-builder" and permissions["project"] and permissions["processes"] and "release-note-builder-v1" in requirements) or (command["action"] == "accessibility-audit" and not permissions["project"] and "accessibility-audit-v1" in requirements) or (command["action"] == "diagram-preview" and not permissions["project"] and "diagram-preview-v1" in requirements) or (command["action"] == "run-rust-ownership" and permissions["project"] and permissions["processes"] and "rust-diagnostic-run-v1" in requirements) or (command["action"] == "review-rust-ownership" and permissions["project"] and "rust-ownership-lens-v1" in requirements) or (command["action"] == "crash-dump-reader" and permissions["project"] and "crash-dump-reader-v1" in requirements) or (command["action"] == "review-linker-map" and permissions["project"] and "linker-map-v1" in requirements) or (command["action"] == "hex-editor" and permissions["project"] and "hex-editor-v1" in requirements) or (command["action"] == "import-junit" and permissions["project"] and "junit-import-v1" in requirements) or (command["action"] == "run-ctest" and permissions["project"] and permissions["processes"] and permissions["settings"] and "ctest-runner-v2" in requirements) or (command["action"] == "install-python-pylsp" and permissions["project"] and permissions["network"] and permissions["processes"] and permissions["settings"] and "python-pylsp-install-v1" in requirements) or (command["action"] == "manage-python-environments" and permissions["project"] and permissions["processes"] and permissions["settings"] and "python-environments-v2" in requirements) or (command["action"] == "browse-python-environments" and permissions["project"] and "python-environments-v1" in requirements) or (command["action"] == "log-correlation-json" and permissions["project"] and "log-json-lines-v1" in requirements) or (command["action"] == "log-correlation" and permissions["project"] and "log-correlation-v1" in requirements) or (command["action"] == "design-token-explorer" and permissions["project"] and "design-token-explorer-v1" in requirements) or (command["action"] == "compare-localization" and permissions["project"] and "localization-workspace-v1" in requirements) or (command["action"] == "binary-inspector" and permissions["project"] and "binary-inspector-v1" in requirements) or (command["action"] == "benchmark-comparison" and permissions["project"] and "benchmark-comparison-v1" in requirements) or (command["action"] == "git-range-history" and permissions["project"] and permissions["processes"] and "git-range-history-v1" in requirements) or (command["action"] == "git-file-history" and permissions["project"] and permissions["processes"] and "git-file-history-v1" in requirements) or (command["action"] in ("explain-diagnostic-local", "explain-debug-local") and not permissions["project"]) or command["action"] in ("markdown-outline", "markdown-preview")) or "insert" in command or "snippet" in command:
                    raise ValueError(f"Invalid module action: {path}")
                continue
            if not isinstance(command.get("insert"), str) or not 1 <= len(command["insert"].encode()) <= 4096:
                raise ValueError(f"Invalid command: {path}")
            if "snippet" in command and not isinstance(command["snippet"], bool):
                raise ValueError(f"Invalid snippet flag: {path}")
            if command.get("snippet") and command["insert"].count("{{cursor}}") > 1:
                raise ValueError(f"Multiple snippet cursor markers: {path}")
            if command.get("snippet") and ("snippet-placeholder-v1" in requirements or "snippet-placeholders-v2" in requirements):
                text = command["insert"]
                count = text.count("{{placeholder:")
                if count > (32 if "snippet-placeholders-v2" in requirements else 1) or ("snippet-placeholders-v2" not in requirements and count and "{{cursor}}" in text):
                    raise ValueError(f"Invalid snippet placeholder: {path}")
                remaining = text
                while "{{placeholder:" in remaining:
                    start = remaining.index("{{placeholder:") + 14
                    end = remaining.find("}}", start)
                    if end < 0:
                        raise ValueError(f"Unclosed snippet placeholder: {path}")
                    remaining = remaining[end + 2:]
            if command.get("snippet") and "snippet-linked-v1" in requirements:
                remaining = command["insert"]
                linked = set()
                count = remaining.count("{{placeholder:")
                while "{{linked:" in remaining:
                    start = remaining.index("{{linked:") + 9
                    end = remaining.find("}}", start)
                    if end < 0:
                        raise ValueError(f"Unclosed linked field: {path}")
                    body = remaining[start:end]
                    field_name, separator, _ = body.partition(":")
                    if not field_name or len(field_name) > 64 or any(not ("A" <= c <= "Z" or "a" <= c <= "z" or "0" <= c <= "9" or c == "_") for c in field_name):
                        raise ValueError(f"Invalid linked field name: {path}")
                    if (separator and field_name in linked) or (not separator and field_name not in linked):
                        raise ValueError(f"Invalid linked field order: {path}")
                    linked.add(field_name)
                    count += 1
                    remaining = remaining[end + 2:]
                if count > 32:
                    raise ValueError(f"Too many snippet fields: {path}")
        if permissions.get("devices", False) and not any(command.get("action") in ("serial-monitor", "board-profile", "flash-firmware", "identify-usb-boards") for command in commands):
            raise ValueError(f"Unsupported device permission: {path}")
        if "cmake-build-target-v1" in requirements and (not permissions["project"] or not permissions["processes"] or "cmake-codemodel-v1" not in requirements or not any(command.get("action") == "browse-cmake" for command in commands)):
            raise ValueError(f"Target build requires CMake browsing and process access: {path}")
        if "cmake-cache-values-v1" in requirements and (not permissions["project"] or "cmake-cache-edit-v1" not in requirements or not any(command.get("action") == "edit-cmake-options" for command in commands)):
            raise ValueError(f"Typed cache editing requires CMake cache edit access: {path}")
        if "toolchain-configure-v1" in requirements and (not permissions["project"] or not permissions["processes"] or "toolchain-profiles-v1" not in requirements or not any(command.get("action") == "inspect-toolchain-profiles" for command in commands)):
            raise ValueError(f"Target configuration requires project, process and toolchain profile access: {path}")
        if "toolchain-target-edit-v1" in requirements and (not permissions["project"] or "toolchain-profiles-v1" not in requirements or not any(command.get("action") == "inspect-toolchain-profiles" for command in commands)):
            raise ValueError(f"Target editing requires project and toolchain profile access: {path}")
        if "openocd-bin-v1" in requirements and ("openocd-flash-v1" not in requirements or not permissions.get("devices", False) or not permissions["project"] or not permissions["processes"] or not any(command.get("action") == "flash-firmware" for command in commands)):
            raise ValueError(f"BIN flashing requires OpenOCD flash capability and permissions: {path}")
        if "serial-continuous-v1" in requirements and (not permissions.get("devices", False) or "microcontroller-serial-v1" not in requirements or not any(command.get("action") == "serial-monitor" for command in commands)):
            raise ValueError(f"Continuous serial monitoring requires device access and serial-monitor: {path}")
        if any(capability in requirements for capability in ("shader-includes-v1", "shader-spirv-inspection-v1", "shader-render-preview-v1")) and (not permissions["project"] or not any(command.get("action") == "compile-shader" for command in commands)):
            raise ValueError(f"Shader inspection requires project access and a shader command: {path}")
        if ("dependency-artifact-sha2-v2" in requirements or "dependency-artifact-go-v1" in requirements) and "dependency-artifact-verify-v1" not in requirements:
            raise ValueError(f"Extended artifact checks require artifact verification: {path}")
        if "dependency-artifact-verify-v1" in requirements and (not permissions["project"] or not permissions["network"] or "dependency-provenance-v1" not in requirements or not any(command.get("action") == "review-dependency-risk" for command in commands)):
            raise ValueError(f"Artifact verification requires project access and dependency provenance: {path}")
        if "localization-edit-v1" in requirements and (not permissions["project"] or not any(command.get("action") == "compare-localization" for command in commands)):
            raise ValueError(f"Localization editing requires project access and a comparison command: {path}")
        if "container-project-actions-v1" in requirements and (not permissions["project"] or not permissions["network"] or not permissions["processes"] or not any(command.get("action") == "configured-container-actions" for command in commands)):
            raise ValueError(f"Configured container actions require project, network and process access: {path}")
        if "api-response-assertions-v1" in requirements and (not permissions["project"] or not permissions["network"] or "api-request-studio-v1" not in requirements or not any(command.get("action") in ("open-api-requests", "open-api-environment") for command in commands)):
            raise ValueError(f"Response assertions require project access and an API request command: {path}")
        if "api-environment-preflight-v1" in requirements and (not permissions["project"] or not permissions["network"] or "api-environments-v1" not in requirements or not any(command.get("action") == "open-api-environment" for command in commands)):
            raise ValueError(f"API preflight requires project access and environment profiles: {path}")
        if "junit-producer-compat-v1" in requirements and (not permissions["project"] or "junit-import-v1" not in requirements or not any(command.get("action") == "import-junit" for command in commands)):
            raise ValueError(f"JUnit producer compatibility requires project access and JUnit import: {path}")
        if "junit-import-v1" in requirements and (not permissions["project"] or not any(command.get("action") == "import-junit" for command in commands)):
            raise ValueError(f"JUnit import requires project access and an import command: {path}")
        if "coverage-cobertura-branches-v1" in requirements and (not permissions["project"] or "coverage-cobertura-v1" not in requirements or not any(command.get("action") == "coverage-cobertura" for command in commands)):
            raise ValueError(f"Cobertura branch coverage requires project access and Cobertura import: {path}")
        if "design-token-aliases-v1" in requirements and (not permissions["project"] or "design-token-explorer-v1" not in requirements or not any(command.get("action") == "design-token-explorer" for command in commands)):
            raise ValueError(f"Design token aliases require project access and token exploration: {path}")
        if "diagram-flow-rounded-v1" in requirements and ("diagram-preview-v1" not in requirements or not any(command.get("action") == "diagram-preview" for command in commands)):
            raise ValueError(f"Diagram node shapes require diagram preview support: {path}")
        if "diagram-flow-branches-v1" in requirements and ("diagram-preview-v1" not in requirements or not any(command.get("action") == "diagram-preview" for command in commands)):
            raise ValueError(f"Diagram branches require diagram preview support: {path}")
        if "diagram-flow-bidirectional-v1" in requirements and ("diagram-preview-v1" not in requirements or "diagram-flow-cycles-v1" not in requirements or not any(command.get("action") == "diagram-preview" for command in commands)):
            raise ValueError(f"Bidirectional diagrams require cyclic diagram preview support: {path}")
        if "diagram-flow-chains-v1" in requirements and ("diagram-preview-v1" not in requirements or not any(command.get("action") == "diagram-preview" for command in commands)):
            raise ValueError(f"Diagram chains require diagram preview support: {path}")
        if "diagram-flow-cycles-v1" in requirements and ("diagram-preview-v1" not in requirements or not any(command.get("action") == "diagram-preview" for command in commands)):
            raise ValueError(f"Cyclic diagrams require diagram preview support: {path}")
        if "benchmark-samples-v1" in requirements and (not permissions["project"] or "benchmark-comparison-v1" not in requirements or not any(command.get("action") == "benchmark-comparison" for command in commands)):
            raise ValueError(f"Benchmark samples require project access and benchmark comparison support: {path}")
        if "log-json-lines-v1" in requirements and (not permissions["project"] or "log-correlation-v1" not in requirements or not any(command.get("action") == "log-correlation-json" for command in commands)):
            raise ValueError(f"JSON log correlation requires project access and log correlation support: {path}")
        if "visual-mask-draw-v1" in requirements and (not permissions["project"] or "visual-config-edit-v1" not in requirements or not any(command.get("action") == "compare-visual-configured" for command in commands)):
            raise ValueError(f"Drawing visual masks requires configured visual editing: {path}")
        if "visual-config-edit-v1" in requirements and (not permissions["project"] or "visual-regression-config-v1" not in requirements or not any(command.get("action") == "compare-visual-configured" for command in commands)):
            raise ValueError(f"Visual config editing requires project access and configured review: {path}")
        if "visual-review-crop-v1" in requirements and (not permissions["project"] or "visual-regression-review-v1" not in requirements or not any(command.get("action") in ("compare-visual-images", "compare-visual-configured") for command in commands)):
            raise ValueError(f"Visual crop requires project access and visual review support: {path}")
        if "sql-change-row-diff-v1" in requirements and (not permissions["project"] or "sql-change-copy-v1" not in requirements or not any(command.get("action") in ("sql-change-preview", "sql-change-bound") for command in commands)):
            raise ValueError(f"SQL row comparison requires project access and change-copy support: {path}")
        if "sql-change-bound-v1" in requirements and (not permissions["project"] or "sql-change-copy-v1" not in requirements or "sql-parameters-v1" not in requirements or not any(command.get("action") == "sql-change-bound" for command in commands)):
            raise ValueError(f"Bound SQL changes require project access, change-copy and parameter capabilities: {path}")
        if "sql-change-copy-v1" in requirements and (not permissions["project"] or not any(command.get("action") == "sql-change-preview" for command in commands)):
            raise ValueError(f"SQL change preview requires project access and a preview command: {path}")
        if "sql-columns-v1" in requirements and (not permissions["project"] or not any(command.get("action") in ("sql-query", "sql-explain", "sql-query-bound", "sql-explain-bound") for command in commands)):
            raise ValueError(f"SQL column inspection requires project access and a query command: {path}")
        if "api-collection-edit-v1" in requirements and not any(command.get("action") == "edit-api-collection" for command in commands):
            raise ValueError(f"API collection editing requires an editing command: {path}")
        if ("dependency-risk-go-v1" in requirements or "dependency-risk-cargo-v1" in requirements or "dependency-provenance-v1" in requirements) and ("dependency-risk-review-v1" not in requirements or not permissions["network"] or not any(command.get("action") == "review-dependency-risk" for command in commands)):
            raise ValueError(f"Extended dependency review requires the dependency review capability and command: {path}")
        if permissions["network"] and not any(command.get("action") in ("markdown-external-links", "install-python-pylsp", "web-preview", "fetch-pull-request", "open-api-requests", "open-api-environment", "review-dependency-risk", "inspect-containers", "configured-container-actions") for command in commands):
            raise ValueError(f"Unsupported network permission: {path}")
        if permissions["settings"] and not any(command.get("action") in ("install-python-pylsp", "manage-python-environments", "run-ctest") for command in commands):
            raise ValueError(f"Unsupported settings permission: {path}")
        if permissions["processes"] and not ("cmake-build-target-v1" in requirements and any(command.get("action") == "browse-cmake" for command in commands)) and not ("toolchain-configure-v1" in requirements and any(command.get("action") == "inspect-toolchain-profiles" for command in commands)) and not any(command.get("action") in ("run-rust-ownership", "flash-firmware", "compile-shader", "inspect-containers", "configured-container-actions", "release-note-builder", "git-file-history", "git-range-history", "install-python-pylsp", "manage-python-environments", "run-ctest") for command in commands):
            raise ValueError(f"Unsupported process permission: {path}")
        if "api-response-save-v1" in requirements and (not permissions["project"] or not any(command.get("action") in ("open-api-requests", "open-api-environment") for command in commands)):
            raise ValueError(f"Response saving requires project permission and an API action: {path}")
        if any(capability in requirements for capability in ("pull-request-comment-v1", "pull-request-review-v1")) and (not permissions["network"] or not any(command.get("action") == "fetch-pull-request" for command in commands)):
            raise ValueError(f"Pull request collaboration requires network access and a pull request action: {path}")
        if permissions["project"] and not any(command.get("action") in ("configured-container-actions", "markdown-external-links", "identify-usb-boards", "inspect-svd-registers", "inspect-toolchain-profiles", "edit-cmake-options", "coverage-cobertura", "open-api-environment", "flash-firmware", "web-preview", "compile-shader", "compare-visual-images", "compare-visual-configured", "sql-change-bound", "sql-change-preview", "sql-query", "sql-explain", "sql-query-bound", "sql-explain-bound", "release-note-builder", "scan-secrets", "browse-cmake", "prepare-cmake-query", "markdown-local-links", "coverage-lcov", "git-file-history", "git-range-history", "benchmark-comparison", "binary-inspector", "compare-localization", "design-token-explorer", "log-correlation-json", "log-correlation", "browse-python-environments", "install-python-pylsp", "manage-python-environments", "import-junit", "run-ctest", "hex-editor", "review-linker-map", "crash-dump-reader", "run-rust-ownership", "review-rust-ownership") or (command.get("action") == "open-api-requests" and ("api-response-save-v1" in requirements or "api-response-assertions-v1" in requirements)) or (command.get("action") == "review-dependency-risk" and "dependency-artifact-verify-v1" in requirements) for command in commands):
            raise ValueError(f"Unsupported project permission: {path}")
        if any(command.get("snippet") for command in commands) and "snippet-v1" not in requirements:
            raise ValueError(f"Snippet capability must be declared: {path}")
        if any(command.get("action") == "scan-secrets" for command in commands) and "project-secret-scan-v1" not in requirements:
            raise ValueError(f"Scan capability must be declared: {path}")
        if any(command.get("action") == "explain-diagnostic-local" for command in commands) and "local-error-explainer-v1" not in requirements:
            raise ValueError(f"Local explanation capability must be declared: {path}")
        if any(command.get("action") == "explain-debug-local" for command in commands) and "local-debug-explainer-v1" not in requirements:
            raise ValueError(f"Local debugger explanation capability must be declared: {path}")
        if any(command.get("action") == "browse-cmake" for command in commands) and "cmake-codemodel-v1" not in requirements:
            raise ValueError(f"CMake capability must be declared: {path}")
        if any(command.get("action") == "prepare-cmake-query" for command in commands) and "cmake-query-v1" not in requirements:
            raise ValueError(f"CMake query capability must be declared: {path}")
        if any(command.get("action") == "markdown-outline" for command in commands) and "markdown-outline-v1" not in requirements:
            raise ValueError(f"Markdown outline capability must be declared: {path}")
        if any(command.get("action") == "markdown-preview" for command in commands) and "markdown-preview-v1" not in requirements:
            raise ValueError(f"Markdown preview capability must be declared: {path}")
        if any(command.get("action") == "markdown-local-links" for command in commands) and "markdown-local-links-v1" not in requirements:
            raise ValueError(f"Markdown link capability must be declared: {path}")
        if any(command.get("action") == "coverage-lcov" for command in commands) and "coverage-lcov-v1" not in requirements:
            raise ValueError(f"LCOV coverage capability must be declared: {path}")
        suffix = module_id.removeprefix("lucid-")
        package = f"{owner}/lucid-module-{suffix}"
        entry = {"id": module_id, "name": name, "description": description, "package": package, "version": version}
        if requirements:
            entry["requires"] = requirements
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
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--catalog-tag", default="v2")
    args = parser.parse_args()
    # OCI tags are limited to 128 characters. Validate before pushing any
    # module artifacts so an unusable catalog tag cannot leave a partial run.
    if len(args.catalog_tag) > 128 or not VERSION.fullmatch(args.catalog_tag) or ".." in args.catalog_tag or args.catalog_tag == "latest":
        raise ValueError("Use a versioned catalog tag; latest is reserved for legacy Lucid builds")
    entries = collect(args.owner)
    catalog = {"modules": entries}
    if args.verify:
        artifacts = [(f"ghcr.io/{entry['package']}:{entry['version']}", f"modules/{entry['id']}/module.json", "application/vnd.lucid.module.v1+json", (MODULES / entry['id'] / 'module.json').read_bytes()) for entry in entries]
        catalog_bytes = (json.dumps(catalog, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        artifacts.append((f"ghcr.io/{args.owner}/lucid-module-catalog:{args.catalog_tag}", "modules/catalog.json", "application/vnd.lucid.catalog.v1+json", catalog_bytes))
        with tempfile.TemporaryDirectory(prefix="lucid-module-verify-") as temporary:
            for index, (reference, source, media_type, expected) in enumerate(artifacts):
                output = Path(temporary) / str(index)
                subprocess.run(["oras", "pull", reference, "--output", str(output)], cwd=ROOT, check=True)
                if expected != (output / source).read_bytes():
                    raise ValueError(f"Published content differs: {reference}")
                metadata = strict_json(subprocess.check_output(["oras", "manifest", "fetch", reference], cwd=ROOT))
                layers = metadata.get("layers", [])
                if len(layers) != 1 or layers[0].get("mediaType") != media_type:
                    raise ValueError(f"Unexpected published layer: {reference}")
        return
    if not args.publish:
        print(json.dumps(catalog, indent=2, ensure_ascii=False))
        return
    # Catalog v2 is a mutable index; individual module version tags are not.
    # This preflight is not an atomic registry compare-and-swap: publication
    # must remain serialized and exclusive to this publisher.
    pending = []
    for entry in entries:
        reference = f"ghcr.io/{entry['package']}:{entry['version']}"
        content = (MODULES / entry["id"] / "module.json").read_bytes()
        if not module_version_exists(reference, content):
            pending.append((entry, content))
    for entry, content in pending:
        if (MODULES / entry["id"] / "module.json").read_bytes() != content:
            raise ValueError(f"Module changed during publication: {entry['id']}")
        source = f"modules/{entry['id']}/module.json:application/vnd.lucid.module.v1+json"
        subprocess.run(["oras", "push", f"ghcr.io/{entry['package']}:{entry['version']}", "--artifact-type", "application/vnd.lucid.module.v1", source], cwd=ROOT, check=True)
    # Write the same UTF-8 bytes used by verification on every platform;
    # text-mode newline translation on Windows would otherwise produce CRLF.
    (MODULES / "catalog.json").write_bytes((json.dumps(catalog, indent=2, ensure_ascii=False) + "\n").encode("utf-8"))
    subprocess.run(["oras", "push", f"ghcr.io/{args.owner}/lucid-module-catalog:{args.catalog_tag}", "--artifact-type", "application/vnd.lucid.catalog.v1", "modules/catalog.json:application/vnd.lucid.catalog.v1+json"], cwd=ROOT, check=True)


if __name__ == "__main__":
    main()
