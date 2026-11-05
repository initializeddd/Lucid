#!/usr/bin/env python3
"""Check that generated catalog references satisfy Lucid's registry syntax.

Keep this predicate in sync with validName in src/modules/Registry.cpp. The
publisher accepts some broader strings, so a valid OCI tag alone is not enough.
"""
import importlib.util
from pathlib import Path
import unittest

SCRIPT = Path(__file__).resolve().parents[2] / "tools/modules/publish.py"
spec = importlib.util.spec_from_file_location("lucid_publish_compat", SCRIPT)
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


def valid_name(value, package):
    def alphanumeric(char):
        return "a" <= char <= "z" or "0" <= char <= "9"

    if not value or len(value) > 160 or not alphanumeric(value[0]) or not alphanumeric(value[-1]) or ".." in value:
        return False
    for i, char in enumerate(value):
        if char == "/":
            if not i or i + 1 == len(value) or not alphanumeric(value[i - 1]) or not alphanumeric(value[i + 1]):
                return False
        elif not alphanumeric(char) and char not in "-_.":
            return False
    return value.count("/") == (1 if package else 0)


class RegistryCompatibilityTest(unittest.TestCase):
    def test_registry_constraints(self):
        for valid in ("v0.2.0-beta.1", "lucid-module-sql-workbench"):
            self.assertTrue(valid_name(valid, False))
        for invalid in ("v1-", "v1_", "v1..0", "../x", "v1/extra"):
            self.assertFalse(valid_name(invalid, False))
        self.assertTrue(valid_name("initializeddd/lucid-module-sql-workbench", True))
        self.assertFalse(valid_name("initializeddd-/lucid-module-sql-workbench", True))

    def test_current_catalog_can_be_parsed_by_registry(self):
        for entry in publisher.collect("initializeddd"):
            with self.subTest(module=entry["id"]):
                self.assertLessEqual(len(entry["id"]), 80)
                self.assertNotIn(".", entry["id"])
                self.assertTrue(valid_name(entry["id"], False))
                self.assertTrue(valid_name(entry["package"], True))
                self.assertTrue(valid_name(entry["version"], False))


if __name__ == "__main__":
    unittest.main()
