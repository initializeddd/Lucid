#!/usr/bin/env python3
"""Test anonymous registry checks without exposing credentials or using GHCR."""
import hashlib
import importlib.util
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.request import Request

TOOLS = Path(__file__).resolve().parents[2] / "tools/modules"
sys.path.insert(0, str(TOOLS))
spec = importlib.util.spec_from_file_location("check_public", TOOLS / "check_public.py")
public = importlib.util.module_from_spec(spec)
spec.loader.exec_module(public)


class PublicCheckTest(unittest.TestCase):
    def test_rejects_oversized_catalog_tag_before_registry_access(self):
        with patch.object(sys, "argv", ["check_public.py", "--catalog-tag", "a" * 129]), \
             patch.object(public, "collect") as collect:
            with self.assertRaises(SystemExit):
                public.main()
            collect.assert_not_called()

    def test_public_payload_and_mismatch(self):
        payload = b'{"name":"example"}'
        media_type = "application/vnd.lucid.module.v1+json"
        digest = "sha256:" + hashlib.sha256(payload).hexdigest()

        def request(url, headers=None):
            if "/token?" in url:
                return b'{"token":"temporary"}'
            self.assertEqual(headers["Authorization"], "Bearer temporary")
            if "/manifests/" in url:
                return json.dumps({"layers": [{"mediaType": media_type, "digest": digest}]}).encode()
            self.assertTrue(url.endswith(digest))
            return payload

        public.check("example/module", "1.0", media_type, payload, request)
        with self.assertRaisesRegex(ValueError, "differs"):
            public.check("example/module", "1.0", media_type, b"wrong", request)

    def test_redirect_drops_token_outside_registry(self):
        redirect = public.SafeRedirect()
        req = Request("https://ghcr.io/v2/x", headers={"Authorization": "Bearer secret"})
        same = redirect.redirect_request(req, None, 302, "", {}, "https://ghcr.io/v2/y")
        other = redirect.redirect_request(req, None, 302, "", {}, "https://cdn.example/y")
        self.assertEqual(same.get_header("Authorization"), "Bearer secret")
        self.assertIsNone(other.get_header("Authorization"))
        with self.assertRaisesRegex(ValueError, "insecure"):
            redirect.redirect_request(req, None, 302, "", {}, "http://cdn.example/y")

    def test_rejects_ambiguous_json_and_invalid_tokens(self):
        media = "application/vnd.lucid.module.v1+json"
        for token in (b'{"token":"a","token":"b"}', b'{"token":"a\\u0000b"}',
                      b'{"token":"space token"}', json.dumps({"token": "x" * 4097}).encode()):
            with self.assertRaises(ValueError):
                public.check("example/module", "1", media, b"", lambda url, headers=None: token)

        def duplicate_manifest(url, headers=None):
            return b'{"token":"t"}' if "/token?" in url else b'{"layers":[],"layers":[]}'
        with self.assertRaisesRegex(ValueError, "Duplicate"):
            public.check("example/module", "1", media, b"", duplicate_manifest)


if __name__ == "__main__":
    unittest.main()
