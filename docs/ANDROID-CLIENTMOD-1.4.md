# ClientMod Rec 1.4 Android resources

Inspected the user-provided `rec1.4(1).apk` as data, without executing it.
The ZIP contains 85 entries, including 26 native libraries for each of
armeabi-v7a and arm64-v8a. ARM64 libraries are Android ELF objects, not iOS
Mach-O dylibs. No C++ source files are supplied by this APK.

`assets/extras_dir.vpk` is a self-contained VPK v2 with 117 files:

- `cfg/touch_default.cfg`: original mobile button layout, sensitivity, movement zones.
- `materials/vgui/touch/*`: buy, jump, crouch, fire, reload, grenade slot, weapon
  slots, drop, use, scoreboard, team/menu/settings, chat and other mobile icons.
- Radar overlay/background and scope textures.
- Touch options and custom chat resource definitions.
- Molotov sound definitions and compiled shader permutations.

The APK also contains real `libclient.so`, `libGameUI.so`, `libengine.so`,
`libmaterialsystem.so`, `libshaderapidx9.so`, `libstdshader_dx9.so`, and `libtogl.so`.
These establish that the Android client has a graphical client path; they cannot
be directly linked into an iOS build. Shader permutation bytecode is not Metal
shader source. This resource import does not port Android GameUI or those binaries.
Maps and agent model assets are absent from this APK's extras archive.

To prepare the existing user-owned cache, run:

```sh
python3 tools/import_android_extras.py CLIENTMOD.apk CONTENT/cm
```

Copy the resulting `extras_dir.vpk` and `cfg/touch_apk.cfg` into the existing
`Documents/Source1IOS/content/cm` folder on the phone. The APK's mobile layout
then takes precedence over the desktop-style `touch.cfg`. Existing touch.cfg is
preserved. Remove touch_apk.cfg to return to that layout. The existing cache
remains required for maps, models and the base game's assets.

The importer reads the ZIP with CRC checking, validates VPK header/tree/entry
bounds, rejects external VPK chunks, and copies only the embedded resources plus
the named touch configuration. It does not extract or execute Android libraries.
