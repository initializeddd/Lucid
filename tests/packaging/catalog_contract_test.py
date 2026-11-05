#!/usr/bin/env python3
"""Round-trip the generated catalog through Lucid's actual registry parser."""
import importlib.util
import json
import subprocess
import sys
import tempfile
from pathlib import Path

executable, root = sys.argv[1:]
root = Path(root)
spec = importlib.util.spec_from_file_location("lucid_publish_contract", root / "tools/modules/publish.py")
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)
catalog = {"modules": publisher.collect("initializeddd")}
with tempfile.TemporaryDirectory(prefix="lucid-catalog-contract-") as temp:
    path = Path(temp) / "catalog.json"
    storage = Path(temp) / "module-storage"
    storage.mkdir()
    path.write_text(json.dumps(catalog), encoding="utf-8")
    subprocess.run([executable, str(root / "modules"), str(path), str(storage)], check=True)
    # An otherwise valid but misleading requirements list must not pass.
    catalog["modules"][0]["requires"] = ["invented-capability-v1"]
    path.write_text(json.dumps(catalog), encoding="utf-8")
    result = subprocess.run([executable, str(root / "modules"), str(path), str(storage)], capture_output=True, check=False)
    if result.returncode == 0:
        raise SystemExit("Mismatched catalog capabilities were not rejected")
