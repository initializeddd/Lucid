#!/usr/bin/env python3
"""Check that every current module and the catalog can be pulled anonymously.

Unlike the authenticated post-publish ORAS check, this tests the same visibility
that Lucid's unauthenticated GitHub catalog and package browser requires.
"""
import argparse
import hashlib
import json
import sys
import urllib.error
import urllib.parse
import urllib.request

from publish import MODULES, VERSION, collect, strict_json

MANIFEST_ACCEPT = "application/vnd.oci.image.manifest.v1+json"
MAX_BYTES = 2 * 1024 * 1024


class SafeRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if urllib.parse.urlsplit(newurl).scheme != "https":
            raise ValueError("Registry returned an insecure redirect")
        redirected = super().redirect_request(req, fp, code, msg, headers, newurl)
        if redirected and urllib.parse.urlsplit(req.full_url).netloc != urllib.parse.urlsplit(newurl).netloc:
            redirected.remove_header("Authorization")
        return redirected


def get(url, headers=None):
    request = urllib.request.Request(url, headers=headers or {})
    with urllib.request.build_opener(SafeRedirect()).open(request, timeout=20) as response:
        data = response.read(MAX_BYTES + 1)
    if len(data) > MAX_BYTES:
        raise ValueError("Registry response exceeded the size limit")
    return data


def check(reference, version, expected_type, expected_bytes, request=get):
    token_url = "https://ghcr.io/token?" + urllib.parse.urlencode({"scope": f"repository:{reference}:pull"})
    token = strict_json(request(token_url))["token"]
    if not isinstance(token, str) or not token or len(token) > 4096 or any(ord(c) < 33 or ord(c) > 126 for c in token):
        raise ValueError("Invalid anonymous registry token")
    headers = {"Authorization": "Bearer " + token}
    base = "https://ghcr.io/v2/" + reference
    manifest = strict_json(request(base + "/manifests/" + version, {**headers, "Accept": MANIFEST_ACCEPT}))
    layers = manifest.get("layers", [])
    if not isinstance(layers, list) or len(layers) != 1 or not isinstance(layers[0], dict) or layers[0].get("mediaType") != expected_type:
        raise ValueError("Unexpected package layers")
    digest = layers[0].get("digest", "")
    if not isinstance(digest, str) or len(digest) != 71 or not digest.startswith("sha256:") or any(c not in "0123456789abcdef" for c in digest[7:]):
        raise ValueError("Invalid layer digest")
    blob = request(base + "/blobs/" + digest, headers)
    if hashlib.sha256(blob).hexdigest() != digest[7:] or blob != expected_bytes:
        raise ValueError("Published layer differs from the local content")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--owner", default="initializeddd")
    parser.add_argument("--catalog-tag", default="v2")
    args = parser.parse_args()
    if len(args.catalog_tag) > 128 or not VERSION.fullmatch(args.catalog_tag) or ".." in args.catalog_tag or args.catalog_tag == "latest":
        parser.error("use a versioned catalog tag other than latest")
    entries = collect(args.owner)
    artifacts = [(entry["package"], entry["version"], "application/vnd.lucid.module.v1+json", (MODULES / entry["id"] / "module.json").read_bytes()) for entry in entries]
    catalog = (json.dumps({"modules": entries}, indent=2, ensure_ascii=False) + "\n").encode()
    artifacts.append((args.owner + "/lucid-module-catalog", args.catalog_tag, "application/vnd.lucid.catalog.v1+json", catalog))
    failures = 0
    for reference, version, media_type, content in artifacts:
        try:
            check(reference, version, media_type, content)
            print(f"OK {reference}:{version}")
        except (OSError, ValueError, KeyError, TypeError, AttributeError, urllib.error.URLError) as error:
            failures += 1
            print(f"FAIL {reference}:{version}: {error}", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
