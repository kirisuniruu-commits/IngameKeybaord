#!/usr/bin/env python3
"""Package libingame_keyboard.so + manifest.json into a .levipack (zip)."""
import argparse
import json
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    ap = argparse.ArgumentParser(description="Package LeviPack")
    ap.add_argument("--library", required=True, help="Path to compiled .so")
    ap.add_argument("--output", required=True, help="Output .levipack path")
    ap.add_argument("--manifest", default=str(ROOT / "manifest.json"))
    args = ap.parse_args()

    lib, manifest_path, out = Path(args.library), Path(args.manifest), Path(args.output)
    if not lib.is_file():
        print(f"Error: library not found: {lib}", file=sys.stderr)
        return 1
    if not manifest_path.is_file():
        print(f"Error: manifest not found: {manifest_path}", file=sys.stderr)
        return 1

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entry = manifest.get("entry")
    if entry != lib.name:
        print(f"Error: manifest entry '{entry}' != library name '{lib.name}'", file=sys.stderr)
        return 1

    out.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        z.write(manifest_path, "manifest.json")
        z.write(lib, entry)
    print(f"Created {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
