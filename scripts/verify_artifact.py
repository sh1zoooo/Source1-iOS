#!/usr/bin/env python3
"""Independently inspect the downloaded Actions ZIP without executing its app."""
import argparse
import hashlib
import io
import json
import plistlib
import struct
import zipfile


def verify(path, version, build, passes, revision, require_static_props=False):
    with zipfile.ZipFile(path) as artifact:
        names = artifact.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate artifact entries")
        ipa_name = next(n for n in names if n.endswith("Source1IOS-unsigned.ipa"))
        manifest_name = next(n for n in names if n.endswith("SHA256SUMS.txt"))
        ipa = artifact.read(ipa_name)
        digest = hashlib.sha256(ipa).hexdigest()
        manifest = artifact.read(manifest_name).decode().splitlines()
        matching = [parts for line in manifest if len(parts := line.split()) == 2 and parts[-1].lstrip("*").endswith("Source1IOS-unsigned.ipa")]
        if len(matching) != 1 or matching[0][0] != digest:
            raise ValueError("IPA SHA256 does not match manifest")
        with zipfile.ZipFile(io.BytesIO(ipa)) as bundle:
            plist = plistlib.loads(bundle.read("Payload/Source1IOS.app/Info.plist"))
            if plist.get("CFBundleShortVersionString") != version or str(plist.get("CFBundleVersion")) != build:
                raise ValueError("Unexpected IPA version/build")
            if plist.get("CFBundleIdentifier") != "io.github.sh1zoooo.source1ios":
                raise ValueError("Unexpected app identifier")
            if not bundle.read("Payload/Source1IOS.app/Source1IOS"):
                raise ValueError("App executable missing")
        log = artifact.read(next(n for n in names if n.endswith("simulator-runtime.log"))).decode()
        if "FAIL" in log or log.count(": PASS") != passes:
            raise ValueError("Unexpected simulator checks or failure")
        required = ["Source Host_Init completed: dedicated idle host", "First Metal frame completed on GPU",
                    f"Source Metal scene completed on GPU: map revision {revision}", "Host paused", "Host resumed",
                    "Source studio external ANI loaded:", "Source content mounted: smoke", "Source content unmounted: smoke"]
        if any(marker not in log for marker in required):
            raise ValueError("Simulator runtime/GPU/lifecycle evidence missing")
        static_props = "Source BSP static props staged: 2 instances, 24 triangles, 1 model types, 0 skipped" in log
        if require_static_props and not static_props:
            raise ValueError("Static prop scene evidence missing")
        png = artifact.read(next(n for n in names if n.endswith("simulator.png")))
        if png[:8] != b"\x89PNG\r\n\x1a\n" or png[12:16] != b"IHDR":
            raise ValueError("Screenshot is not a PNG")
        dimensions = struct.unpack(">II", png[16:24])
        if not all(dimensions):
            raise ValueError("Empty screenshot")
        return {"version": version, "build": build, "ipa_sha256": digest,
                "simulator_passes": passes, "gpu_map_revision": revision,
                "static_props": static_props,
                "screenshot_size": dimensions}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive")
    parser.add_argument("--version", required=True)
    parser.add_argument("--build", required=True)
    parser.add_argument("--passes", required=True, type=int)
    parser.add_argument("--revision", required=True, type=int)
    parser.add_argument("--require-static-props", action="store_true")
    args = parser.parse_args()
    print(json.dumps(verify(args.archive, args.version, args.build, args.passes, args.revision, args.require_static_props), indent=2))
