#!/usr/bin/env python3
"""Exercise module publication and verification without a registry account."""
import importlib.util
import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[2] / "tools/modules/publish.py"
spec = importlib.util.spec_from_file_location("lucid_module_publish", SCRIPT)
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class PublishTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.modules = self.root / "modules"
        directory = self.modules / "lucid-diagram-preview"
        directory.mkdir(parents=True)
        self.manifest = directory / "module.json"
        self.manifest.write_text(json.dumps({
            "name": "Diagram", "version": "0.1.0", "description": "Local preview",
            "requires": ["diagram-preview-v1"],
            "permissions": {"project": False, "network": False, "processes": False, "settings": False},
            "commands": [{"title": "Preview", "action": "diagram-preview"}],
        }))
        self.registry = {}

    def run_oras(self, args, **kwargs):
        if args[1] == "push":
            path, media_type = args[-1].split(":", 1)
            self.registry[args[2]] = (path, (self.root / path).read_bytes(), media_type)
        elif args[1] == "pull":
            path, content, _ = self.registry[args[2]]
            target = Path(args[args.index("--output") + 1]) / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(content)
        else:
            raise AssertionError(args)

    def fetch_manifest(self, args, **kwargs):
        if args[-1] not in self.registry:
            raise subprocess.CalledProcessError(1, args, stderr=b"404 Not Found")
        _, content, media_type = self.registry[args[-1]]
        return json.dumps({"layers": [{"mediaType": media_type, "size": len(content),
                                      "digest": "sha256:" + hashlib.sha256(content).hexdigest()}]}).encode()

    def invoke(self, flag):
        with patch.object(publisher, "ROOT", self.root), patch.object(publisher, "MODULES", self.modules), \
             patch.object(publisher.subprocess, "run", self.run_oras), \
             patch.object(publisher.subprocess, "check_output", self.fetch_manifest), \
             patch.object(sys, "argv", [str(SCRIPT), "--owner", "test-owner", flag]):
            publisher.main()

    def test_publish_verify_and_detect_changed_artifact(self):
        self.invoke("--publish")
        catalog = json.loads((self.modules / "catalog.json").read_text())
        expected = (json.dumps(catalog, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        self.assertEqual((self.modules / "catalog.json").read_bytes(), expected)
        self.assertEqual(catalog["modules"][0]["package"], "test-owner/lucid-module-diagram-preview")
        self.invoke("--verify")
        self.manifest.write_text(self.manifest.read_text() + " ")
        with self.assertRaisesRegex(ValueError, "Published content differs"):
            self.invoke("--verify")

    def test_existing_versions_are_idempotent_and_different_bytes_are_rejected(self):
        self.invoke("--publish")
        before = dict(self.registry)
        self.invoke("--publish")
        self.assertEqual(self.registry, before)
        self.manifest.write_text(self.manifest.read_text() + " ")
        with self.assertRaisesRegex(ValueError, "different content"):
            self.invoke("--publish")
        self.assertEqual(self.registry, before)

    def test_preflight_fails_closed_for_authentication_or_transport_error(self):
        for detail in (b"403 Forbidden", b"connection refused", b"not found"):
            with self.subTest(detail=detail), patch.object(self, "fetch_manifest", side_effect=
                    subprocess.CalledProcessError(1, ["oras"], stderr=detail)):
                with self.assertRaisesRegex(ValueError, "Cannot establish"):
                    self.invoke("--publish")
                self.assertFalse(self.registry)

    def test_all_versions_are_checked_before_any_new_module_is_pushed(self):
        self.invoke("--publish")
        original = dict(self.registry)
        self.manifest.write_text(self.manifest.read_text() + " ")
        first = self.modules / "lucid-aaa"
        first.mkdir()
        (first / "module.json").write_text(self.manifest.read_text())
        with self.assertRaisesRegex(ValueError, "different content"):
            self.invoke("--publish")
        self.assertEqual(self.registry, original)

    def test_reject_undeclared_preview_capability(self):
        data = json.loads(self.manifest.read_text())
        data["requires"] = []
        self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid module action"):
            self.invoke("--publish")
        self.assertEqual(self.registry, {})

    def test_reject_symlinked_module_instead_of_silently_omitting_it(self):
        link = self.modules / "lucid-omitted"
        try:
            link.symlink_to(self.manifest.parent, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest("Directory symlinks unavailable")
        with self.assertRaisesRegex(ValueError, "Unsafe symlink"):
            self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_oversized_catalog_tag_is_rejected_before_any_push(self):
        with patch.object(publisher, "ROOT", self.root), patch.object(publisher, "MODULES", self.modules), \
             patch.object(publisher.subprocess, "run", self.run_oras), \
             patch.object(sys, "argv", [str(SCRIPT), "--owner", "test-owner", "--publish", "--catalog-tag", "a" * 129]):
            with self.assertRaisesRegex(ValueError, "versioned catalog tag"):
                publisher.main()
        self.assertFalse(self.registry)

    def test_reject_names_and_tags_the_installer_cannot_read(self):
        for version in ("1..2", "1.0-", "1.0_", "1.0."):
            data = json.loads(self.manifest.read_text())
            data["version"] = version
            self.manifest.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "Invalid name or version"):
                self.invoke("--publish")
        data["version"] = "1.0"
        self.manifest.write_text(json.dumps(data))
        self.manifest.parent.rename(self.modules / "lucid-invalid-")
        with self.assertRaisesRegex(ValueError, "Invalid module ID"):
            self.invoke("--publish")
        with patch.object(publisher, "MODULES", self.modules):
            with self.assertRaisesRegex(ValueError, "Invalid GitHub package owner"):
                publisher.collect("bad-")

    def test_verify_uses_current_manifests_not_stale_catalog_file(self):
        self.invoke("--publish")
        data = json.loads(self.manifest.read_text())
        data["description"] = "Changed description"
        self.manifest.write_text(json.dumps(data))
        reference = "ghcr.io/test-owner/lucid-module-diagram-preview:0.1.0"
        path, _, media_type = self.registry[reference]
        self.registry[reference] = (path, self.manifest.read_bytes(), media_type)
        # The local catalog still equals the stale registry catalog; verification
        # must compare the registry against the catalog computed from manifests.
        with self.assertRaisesRegex(ValueError, "lucid-module-catalog"):
            self.invoke("--verify")
        (self.modules / "catalog.json").unlink()
        with self.assertRaisesRegex(ValueError, "lucid-module-catalog"):
            self.invoke("--verify")

    def test_reject_ambiguous_json_and_long_installer_version(self):
        for contents in (
            '{"name":"One","name":"Two"}',
            '{"name":"One","nested":{"value":1,"value":2}}',
            '{"name":"One","extra":NaN}',
        ):
            self.manifest.write_text(contents)
            with self.assertRaises(ValueError):
                self.invoke("--publish")
            self.assertEqual(self.registry, {})
        data = {"name": "Diagram", "version": "1" * 65,
                "permissions": {"project": False, "network": False, "processes": False, "settings": False},
                "commands": [{"title": "Insert", "insert": "x"}]}
        self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid name or version"):
            self.invoke("--publish")

    def test_python_server_requires_declared_network_and_capability(self):
        data = {"name": "Python", "version": "1", "requires": ["python-environments-v2", "python-pylsp-install-v1"],
                "permissions": {"project": True, "network": True, "processes": True, "settings": True},
                "commands": [{"title": "Manage", "action": "manage-python-environments"}, {"title": "Install", "action": "install-python-pylsp"}]}
        self.manifest.write_text(json.dumps(data))
        self.invoke("--publish")
        self.registry.clear()
        data["permissions"]["network"] = False
        self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid module action"):
            self.invoke("--publish")
        self.assertFalse(self.registry)
        data["permissions"]["network"] = True
        data["requires"].remove("python-pylsp-install-v1")
        self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid module action"):
            self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_single_action_modules_can_use_their_required_permissions(self):
        for action, capability, permissions in (
            ("identify-usb-boards", "usb-board-identification-v1",
             {"project": True, "network": False, "processes": False, "settings": False, "devices": True}),
            ("inspect-svd-registers", "svd-registers-v1",
             {"project": True, "network": False, "processes": False, "settings": False}),
            ("inspect-toolchain-profiles", "toolchain-profiles-v1",
             {"project": True, "network": False, "processes": False, "settings": False}),
            ("board-profile", "microcontroller-serial-v1",
             {"project": False, "network": False, "processes": False, "settings": False, "devices": True}),
            ("install-python-pylsp", "python-pylsp-install-v1",
             {"project": True, "network": True, "processes": True, "settings": True}),
        ):
            with self.subTest(action=action):
                self.registry.clear()
                self.manifest.write_text(json.dumps({
                    "name": "Focused module", "version": "1", "requires": [capability],
                    "permissions": permissions, "commands": [{"title": "Run", "action": action}],
                }))
                self.invoke("--publish")
                self.invoke("--verify")

    def test_cargo_diagnostic_run_requires_process_project_and_capability(self):
        data={"name":"Rust","version":"1","requires":["rust-diagnostic-run-v1"],"permissions":{"project":True,"network":False,"processes":True,"settings":False},"commands":[{"title":"Run","action":"run-rust-ownership"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.invoke("--verify");self.registry.clear()
        data["permissions"]["processes"]=False;self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Invalid module action"):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_cmake_target_build_requires_browsing_and_process_access(self):
        data={"name":"CMake","version":"1","requires":["cmake-codemodel-v1","cmake-build-target-v1"],"permissions":{"project":True,"network":False,"processes":True,"settings":False},"commands":[{"title":"Browse","action":"browse-cmake"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.invoke("--verify");self.registry.clear()
        data["permissions"]["processes"]=False;self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Target build"):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_typed_cache_requires_cache_edit_capability(self):
        data={"name":"CMake","version":"1","requires":["cmake-cache-edit-v1","cmake-cache-values-v1"],"permissions":{"project":True,"network":False,"processes":False,"settings":False},"commands":[{"title":"Edit","action":"edit-cmake-options"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.invoke("--verify");self.registry.clear()
        data["requires"].remove("cmake-cache-edit-v1");self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Invalid module action|Typed cache"):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_toolchain_configure_requires_process_project_and_profile_capability(self):
        data={"name":"Targets","version":"1","requires":["toolchain-profiles-v1","toolchain-configure-v1"],"permissions":{"project":True,"network":False,"processes":True,"settings":False},"commands":[{"title":"Profiles","action":"inspect-toolchain-profiles"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.invoke("--verify");self.registry.clear()
        data["permissions"]["processes"]=False;self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Target configuration"):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_toolchain_target_edit_requires_profile_capability_and_action(self):
        data={"name":"Targets","version":"1","requires":["toolchain-profiles-v1","toolchain-target-edit-v1"],"permissions":{"project":True,"network":False,"processes":False,"settings":False},"commands":[{"title":"Profiles","action":"inspect-toolchain-profiles"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.invoke("--verify");self.registry.clear()
        data["requires"].remove("toolchain-profiles-v1");self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError,"Invalid module action|Target editing"):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_bin_flash_capability_requires_flash_action_and_permissions(self):
        data = {"name": "Flash", "version": "1", "requires": ["openocd-flash-v1", "openocd-bin-v1"],
                "permissions": {"project": True, "network": False, "processes": True, "settings": False, "devices": True},
                "commands": [{"title": "Flash", "action": "flash-firmware"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.invoke("--verify"); self.registry.clear()
        data["requires"].remove("openocd-flash-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid module action|BIN flashing"): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_continuous_serial_requires_monitor_capability_and_device_permission(self):
        data = {"name": "Monitor", "version": "1", "requires": ["microcontroller-serial-v1", "serial-continuous-v1"],
                "permissions": {"project": False, "network": False, "processes": False, "settings": False, "devices": True},
                "commands": [{"title": "Monitor", "action": "serial-monitor"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.invoke("--verify"); self.registry.clear()
        data["requires"].remove("microcontroller-serial-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, "Invalid module action|Continuous serial"): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_usb_board_identification_requires_project_devices_and_capability(self):
        data = {"name": "USB", "version": "1", "requires": ["usb-board-identification-v1"],
                "permissions": {"project": True, "network": False, "processes": False, "settings": False, "devices": True},
                "commands": [{"title": "Identify", "action": "identify-usb-boards"}]}
        for missing in ("project", "devices", "capability"):
            with self.subTest(missing=missing):
                candidate = json.loads(json.dumps(data))
                if missing == "capability": candidate["requires"] = []
                else: candidate["permissions"][missing] = False
                self.manifest.write_text(json.dumps(candidate))
                with self.assertRaisesRegex(ValueError, "Invalid module action"): self.invoke("--publish")
                self.assertFalse(self.registry)

    def test_svd_registers_require_project_access_and_capability(self):
        data = {"name": "SVD", "version": "1", "requires": ["svd-registers-v1"],
                "permissions": {"project": True, "network": False, "processes": False, "settings": False},
                "commands": [{"title": "Inspect", "action": "inspect-svd-registers"}]}
        for missing in ("project", "capability"):
            with self.subTest(missing=missing):
                candidate = json.loads(json.dumps(data))
                if missing == "project":
                    candidate["permissions"]["project"] = False
                else:
                    candidate["requires"] = []
                self.manifest.write_text(json.dumps(candidate))
                with self.assertRaisesRegex(ValueError, "Invalid module action"):
                    self.invoke("--publish")
                self.assertFalse(self.registry)

    def test_toolchain_profile_requires_project_access_and_capability(self):
        data = {"name": "Toolchain", "version": "1", "requires": ["toolchain-profiles-v1"],
                "permissions": {"project": True, "network": False, "processes": False, "settings": False},
                "commands": [{"title": "Inspect", "action": "inspect-toolchain-profiles"}]}
        for missing in ("project", "capability"):
            with self.subTest(missing=missing):
                candidate = json.loads(json.dumps(data))
                if missing == "project":
                    candidate["permissions"]["project"] = False
                else:
                    candidate["requires"] = []
                self.manifest.write_text(json.dumps(candidate))
                with self.assertRaisesRegex(ValueError, "Invalid module action"):
                    self.invoke("--publish")
                self.assertFalse(self.registry)

    def test_api_response_save_requires_api_command_and_project_access(self):
        data = {"name": "API", "version": "1", "requires": ["api-request-studio-v1", "api-response-save-v1"],
                "permissions": {"project": True, "network": True, "processes": False, "settings": False},
                "commands": [{"title": "Request", "action": "open-api-requests"}]}
        self.manifest.write_text(json.dumps(data))
        self.invoke("--publish")
        self.invoke("--verify")
        self.registry.clear()
        data["permissions"]["project"] = False
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            self.invoke("--publish")
        self.assertFalse(self.registry)
        data["permissions"] = {"project": True, "network": False, "processes": False, "settings": False}
        data["commands"] = [{"title": "Scan", "action": "scan-secrets"}]
        data["requires"] = ["api-response-save-v1", "project-secret-scan-v1"]
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_cobertura_branches_require_cobertura_import(self):
        data = {"name":"Coverage", "version":"1", "requires":["coverage-cobertura-v1","coverage-cobertura-branches-v1"], "permissions":{"project":True,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Import","action":"coverage-cobertura"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("coverage-cobertura-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_design_aliases_require_token_explorer(self):
        data = {"name":"Tokens", "version":"1", "requires":["design-token-explorer-v1","design-token-aliases-v1"], "permissions":{"project":True,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Explore","action":"design-token-explorer"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("design-token-explorer-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_diagram_branches_require_preview_capability(self):
        data = {"name":"Diagram", "version":"1", "requires":["diagram-preview-v1","diagram-flow-branches-v1"], "permissions":{"project":False,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Preview","action":"diagram-preview"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("diagram-preview-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_diagram_bidirectional_requires_cycle_capability(self):
        data = {"name":"Diagram", "version":"1", "requires":["diagram-preview-v1","diagram-flow-cycles-v1","diagram-flow-bidirectional-v1"], "permissions":{"project":False,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Preview","action":"diagram-preview"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("diagram-flow-cycles-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_diagram_chains_require_preview_capability(self):
        data = {"name":"Diagram", "version":"1", "requires":["diagram-preview-v1","diagram-flow-chains-v1"], "permissions":{"project":False,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Preview","action":"diagram-preview"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("diagram-preview-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_diagram_cycles_require_preview_capability(self):
        data = {"name":"Diagram", "version":"1", "requires":["diagram-preview-v1","diagram-flow-cycles-v1"], "permissions":{"project":False,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Preview","action":"diagram-preview"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("diagram-preview-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_benchmark_samples_require_comparison_capability(self):
        data = {"name":"Benchmark", "version":"1", "requires":["benchmark-comparison-v1","benchmark-samples-v1"], "permissions":{"project":True,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Compare","action":"benchmark-comparison"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("benchmark-comparison-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_json_logs_require_correlation_capability(self):
        data = {"name":"Logs", "version":"1", "requires":["log-correlation-v1","log-json-lines-v1"], "permissions":{"project":True,"network":False,"processes":False,"settings":False}, "commands":[{"title":"JSON","action":"log-correlation-json"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("log-correlation-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_visual_draw_mask_requires_config_edit(self):
        data={"name":"Visual","version":"1","requires":["visual-regression-config-v1","visual-config-edit-v1","visual-mask-draw-v1"],"permissions":{"project":True,"network":False,"processes":False,"settings":False},"commands":[{"title":"Configured","action":"compare-visual-configured"}]}
        self.manifest.write_text(json.dumps(data));self.invoke("--publish");self.registry.clear();data["requires"].remove("visual-config-edit-v1");self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_visual_config_edit_requires_configured_review(self):
        data = {"name":"Visual", "version":"1", "requires":["visual-regression-review-v1","visual-regression-config-v1","visual-config-edit-v1"], "permissions":{"project":True,"network":False,"processes":False,"settings":False}, "commands":[{"title":"Configured","action":"compare-visual-configured"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("visual-regression-config-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_visual_crop_requires_visual_review_capability(self):
        data = {"name":"Visual", "version":"1", "requires":["visual-regression-review-v1","visual-review-crop-v1"],
                "permissions":{"project":True,"network":False,"processes":False,"settings":False},
                "commands":[{"title":"Compare","action":"compare-visual-images"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("visual-regression-review-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_sql_row_review_requires_change_copy_capability(self):
        data = {"name":"SQL", "version":"1", "requires":["sql-change-copy-v1","sql-change-row-diff-v1"],
                "permissions":{"project":True,"network":False,"processes":False,"settings":False},
                "commands":[{"title":"Preview","action":"sql-change-preview"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("sql-change-copy-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_bound_sql_changes_require_supporting_capabilities(self):
        data = {"name":"SQL", "version":"1", "requires":["sql-change-copy-v1","sql-parameters-v1","sql-change-bound-v1"],
                "permissions":{"project":True,"network":False,"processes":False,"settings":False},
                "commands":[{"title":"Preview","action":"sql-change-preview"},{"title":"Bound","action":"sql-change-bound"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.registry.clear()
        data["requires"].remove("sql-parameters-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_api_assertions_require_project_and_request_capability(self):
        data = {"name": "API", "version": "1", "requires": ["api-request-studio-v1", "api-response-assertions-v1"],
                "permissions": {"project": True, "network": True, "processes": False, "settings": False},
                "commands": [{"title": "Request", "action": "open-api-requests"}]}
        self.manifest.write_text(json.dumps(data))
        self.invoke("--publish")
        self.registry.clear()
        data["permissions"]["project"] = False
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_api_template_editor_and_shader_include_require_declared_capabilities(self):
        data = {"name": "Editor", "version": "1", "requires": ["api-collection-edit-v1"],
                "permissions": {"project": False, "network": False, "processes": False, "settings": False},
                "commands": [{"title": "Edit", "action": "edit-api-collection"}]}
        self.manifest.write_text(json.dumps(data))
        self.invoke("--publish")
        self.registry.clear()
        data["requires"] = []
        self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            self.invoke("--publish")
        for capability in ("shader-includes-v1", "shader-spirv-inspection-v1", "shader-render-preview-v1", "sql-columns-v1", "localization-edit-v1"):
            data["requires"] = [capability]
            data["commands"] = [{"title": "Insert", "insert": "x"}]
            self.manifest.write_text(json.dumps(data))
            with self.assertRaises(ValueError):
                self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_shader_inspection_requires_compile_action_and_permissions(self):
        for capability in ("shader-includes-v1", "shader-spirv-inspection-v1", "shader-render-preview-v1"):
            with self.subTest(capability=capability):
                data = {"name": "Shader", "version": "1", "requires": ["shader-compile-v1", capability],
                        "permissions": {"project": True, "network": False, "processes": True, "settings": False},
                        "commands": [{"title": "Compile", "action": "compile-shader"}]}
                self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.invoke("--verify"); self.registry.clear()
                for permission in ("project", "processes"):
                    data["permissions"][permission] = False
                    self.manifest.write_text(json.dumps(data))
                    with self.assertRaises(ValueError): self.invoke("--publish")
                    data["permissions"][permission] = True
                data["permissions"]["processes"] = False
                data["commands"] = [{"title": "Scan", "action": "scan-secrets"}]
                data["requires"].append("project-secret-scan-v1")
                self.manifest.write_text(json.dumps(data))
                with self.assertRaisesRegex(ValueError, "Shader inspection requires"): self.invoke("--publish")
                self.assertFalse(self.registry)

    def test_configured_container_action_requires_project_network_process_and_capability(self):
        data = {"name": "Containers", "version": "1", "requires": ["container-inspection-v1", "container-project-actions-v1"],
                "permissions": {"project": True, "network": True, "processes": True, "settings": False},
                "commands": [{"title": "Inspect", "action": "inspect-containers"}, {"title": "Actions", "action": "configured-container-actions"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.invoke("--verify"); self.registry.clear()
        for permission in ("project", "network", "processes"):
            data["permissions"][permission] = False
            self.manifest.write_text(json.dumps(data))
            with self.assertRaises(ValueError): self.invoke("--publish")
            data["permissions"][permission] = True
        data["requires"].remove("container-project-actions-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_dependency_artifact_verification_requires_project_and_provenance(self):
        data = {"name": "Dependency", "version": "1", "requires": ["dependency-risk-review-v1", "dependency-provenance-v1", "dependency-artifact-verify-v1", "dependency-artifact-sha2-v2"],
                "permissions": {"project": True, "network": True, "processes": False, "settings": False},
                "commands": [{"title": "Review", "action": "review-dependency-risk"}]}
        self.manifest.write_text(json.dumps(data)); self.invoke("--publish"); self.invoke("--verify"); self.registry.clear()
        data["permissions"]["project"] = False; self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        data["permissions"]["project"] = True; data["requires"].remove("dependency-provenance-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        data["requires"].append("dependency-provenance-v1"); data["requires"].remove("dependency-artifact-verify-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        data["requires"].remove("dependency-artifact-sha2-v2"); data["requires"].append("dependency-artifact-go-v1"); self.manifest.write_text(json.dumps(data))
        with self.assertRaises(ValueError): self.invoke("--publish")
        self.assertFalse(self.registry)

    def test_pull_request_collaboration_requires_network_and_fetch_action(self):
        for capability in ("pull-request-comment-v1", "pull-request-review-v1"):
            with self.subTest(capability=capability):
                data = {"name": "Review", "version": "1", "requires": ["pull-request-companion-v1", capability],
                        "permissions": {"project": False, "network": True, "processes": False, "settings": False},
                        "commands": [{"title": "Review", "action": "fetch-pull-request"}]}
                self.manifest.write_text(json.dumps(data))
                self.invoke("--publish")
                self.invoke("--verify")
                self.registry.clear()
                data["permissions"]["network"] = False
                self.manifest.write_text(json.dumps(data))
                with self.assertRaises(ValueError):
                    self.invoke("--publish")
                data["permissions"]["network"] = True
                data["requires"].append("api-request-studio-v1")
                data["commands"] = [{"title": "API", "action": "open-api-requests"}]
                self.manifest.write_text(json.dumps(data))
                with self.assertRaisesRegex(ValueError, "Pull request collaboration requires"):
                    self.invoke("--publish")
                self.assertFalse(self.registry)


if __name__ == "__main__":
    unittest.main()
