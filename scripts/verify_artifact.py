#!/usr/bin/env python3
"""Independently inspect the downloaded Actions ZIP without executing its app."""
import argparse
import hashlib
import io
import json
import plistlib
import struct
import zipfile


def verify(path, version, build, passes, revision, require_static_props=False, require_mdl48=False, require_prop_collision=False, require_phy=False, require_hdr=False, require_model_materials=False, require_skins=False, require_vpk_cache=False, require_mdl44=False):
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
        mdl48 = "Source studio MDL version: 48" in log and "from models/__source1ios_external48_probe.mdl" in log
        if require_mdl48 and not mdl48:
            raise ValueError("MDL48 animation scene evidence missing")
        prop_collision = all(marker in log for marker in [
            "Source BSP static prop collision ready: 2 SOLID_BBOX objects, 0 unsupported/degenerate",
            "live static prop vphysics objects: PASS", "live static prop ray and swept hull: PASS",
            "live static prop blocks camera without tunneling: PASS"])
        if require_prop_collision and not prop_collision:
            raise ValueError("Static prop collision evidence missing")
        phy = all(marker in log for marker in ["Source BSP exact PHY collision ready: 2 SOLID_VPHYSICS objects",
                  "live PHY static vphysics objects: PASS", "live PHY tapered shape differs from bounding box: PASS",
                  "live PHY swept camera blocks without tunneling: PASS"])
        if require_phy and not phy:
            raise ValueError("Exact PHY collision evidence missing")
        hdr = all(marker in log for marker in ["Source BSP HDR preview lightmap atlas ready: 42 faces, 1024x1024 RGBA",
                  "live HDR-only faces lighting and PHY scene: PASS", "Source BSP preview lightmap atlas uploaded to Metal"])
        if require_hdr and not hdr:
            raise ValueError("HDR-only preview scene evidence missing")
        model_materials=all(marker in log for marker in ["Source studio material atlas ready: 2 slots, 128x64 RGBA",
                       "from models/__source1ios_multimat_probe.mdl", "live studio per-mesh material slots: PASS",
                       "live studio two VMT VTF atlas tiles: PASS", "live studio skinning preserves material slots: PASS"])
        if require_model_materials and not model_materials:
            raise ValueError("Multi-material studio evidence missing")
        skins=all(marker in log for marker in ["Source studio skin selected: 1; geometry animation and texture atlas retained",
                  "live studio skin family selected: PASS", "live studio skin changes per-mesh texture assignment: PASS",
                  "live studio skin variant preserves weighted animation: PASS", "live static props different skins same model: PASS",
                  "live static prop skin families use distinct texture assignments: PASS", "live static prop skin variants retain PHY objects: PASS"])
        if require_skins and not skins:
            raise ValueError("Studio/static prop skin family evidence missing")
        vpk_cache=all(marker in log for marker in ["Source content mounted: smoke", "1 bounded VPK archives, 0 chunks",
                      "Source filesystem self-test mounted VPK v2 embedded file: PASS"])
        if require_vpk_cache and not vpk_cache:
            raise ValueError("Bounded nested VPK cache evidence missing")
        mdl44 = "studio MDL44 geometry and embedded clip: PASS" in log
        if require_mdl44 and not mdl44:
            raise ValueError("Legacy CS:S MDL44 compatibility evidence missing")
        png = artifact.read(next(n for n in names if n.endswith("simulator.png")))
        if png[:8] != b"\x89PNG\r\n\x1a\n" or png[12:16] != b"IHDR":
            raise ValueError("Screenshot is not a PNG")
        dimensions = struct.unpack(">II", png[16:24])
        if not all(dimensions):
            raise ValueError("Empty screenshot")
        return {"version": version, "build": build, "ipa_sha256": digest,
                "simulator_passes": passes, "gpu_map_revision": revision,
                "static_props": static_props, "mdl48": mdl48, "prop_collision": prop_collision, "phy": phy, "hdr": hdr, "model_materials": model_materials, "skins": skins, "vpk_cache": vpk_cache, "mdl44": mdl44,
                "screenshot_size": dimensions}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive")
    parser.add_argument("--version", required=True)
    parser.add_argument("--build", required=True)
    parser.add_argument("--passes", required=True, type=int)
    parser.add_argument("--revision", required=True, type=int)
    parser.add_argument("--require-static-props", action="store_true")
    parser.add_argument("--require-mdl48", action="store_true")
    parser.add_argument("--require-prop-collision", action="store_true")
    parser.add_argument("--require-phy", action="store_true")
    parser.add_argument("--require-hdr", action="store_true")
    parser.add_argument("--require-model-materials", action="store_true")
    parser.add_argument("--require-skins", action="store_true")
    parser.add_argument("--require-vpk-cache", action="store_true")
    parser.add_argument("--require-mdl44", action="store_true")
    args = parser.parse_args()
    print(json.dumps(verify(args.archive, args.version, args.build, args.passes, args.revision, args.require_static_props, args.require_mdl48, args.require_prop_collision, args.require_phy, args.require_hdr, args.require_model_materials, args.require_skins, args.require_vpk_cache, args.require_mdl44), indent=2))
