#!/usr/bin/env python3
"""Package a built app for external signing; this does not sign it."""
import argparse
from pathlib import Path
import plistlib
import zipfile
parser = argparse.ArgumentParser()
parser.add_argument("app", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
app = args.app.resolve()
if not app.is_dir() or app.suffix != ".app":
    parser.error("Expected an existing .app bundle")
with (app / "Info.plist").open("rb") as stream:
    info = plistlib.load(stream)
executable = info.get("CFBundleExecutable", "")
if not executable or not (app / executable).is_file():
    parser.error("App bundle executable is missing")
if (app / "embedded.mobileprovision").exists() or (app / "_CodeSignature").exists():
    parser.error("Expected an unsigned app bundle")
args.output.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(app.rglob("*")):
        if path.is_file():
            archive.write(path, Path("Payload") / app.name / path.relative_to(app))
with zipfile.ZipFile(args.output) as archive:
    assert archive.testzip() is None, "Corrupt IPA archive"
print(f"Created unsigned IPA: {args.output}")
