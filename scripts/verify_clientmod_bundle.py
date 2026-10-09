#!/usr/bin/env python3
"""Check device architecture, original exports and bundled dynamic dependencies."""
import argparse
from pathlib import Path
import plistlib
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('app', type=Path)
args = parser.parse_args()
app = args.app.resolve()
with (app / 'Info.plist').open('rb') as stream:
    info = plistlib.load(stream)
executable = app / info['CFBundleExecutable']
libraries = app / 'Frameworks'
modules = ('engine', 'client', 'server', 'GameUI', 'vgui2', 'vguimatsurface',
           'materialsystem', 'shaderapidx9', 'stdshader_dx9', 'togl', 'inputsystem',
           'launcher', 'video_services', 'filesystem_stdio', 'datacache',
           'studiorender', 'vphysics', 'soundemittersystem', 'scenefilecache',
           'tier0', 'vstdlib', 'serverbrowser')
for module in modules:
    if not (libraries / f'lib{module}.dylib').is_file():
        raise SystemExit(f'Missing original runtime module: {module}')
if not list(libraries.glob('libSDL*.dylib')):
    raise SystemExit('Missing SDL UIKit provider')
binaries = [executable, *sorted(libraries.glob('*.dylib'))]
for binary in binaries:
    arch = subprocess.check_output(['xcrun', 'lipo', '-archs', str(binary)], text=True).split()
    if arch != ['arm64']:
        raise SystemExit(f'Expected device ARM64: {binary.name}: {arch}')
    dependencies = subprocess.check_output(['xcrun', 'otool', '-L', str(binary)], text=True)
    for line in dependencies.splitlines()[1:]:
        dependency = line.strip().split(' (', 1)[0]
        if dependency.startswith(('/usr/lib/', '/System/Library/')):
            continue
        if dependency.startswith(('@rpath/', '@loader_path/', '@executable_path/')):
            if not (libraries / Path(dependency).name).is_file():
                raise SystemExit(f'{binary.name}: missing bundled dependency {dependency}')
            continue
        raise SystemExit(f'{binary.name}: nonportable dependency {dependency}')
    if binary.name.startswith('lib') and not binary.name.startswith(('libSDL', 'libtier0')):
        symbols = subprocess.check_output(['xcrun', 'nm', '-gU', str(binary)], text=True)
        if not any(line.split()[-1] == '_CreateInterface' for line in symbols.splitlines() if line.split()):
            raise SystemExit(f'Original module factory export missing: {binary.name}')
launcher_symbols = subprocess.check_output(['xcrun', 'nm', '-gU', str(libraries / 'liblauncher.dylib')], text=True)
if not any(line.split()[-1] == '_LauncherMain' for line in launcher_symbols.splitlines() if line.split()):
    raise SystemExit('Original LauncherMain export missing')
main_commands = subprocess.check_output(['xcrun', 'otool', '-l', str(executable)], text=True)
if '@executable_path/Frameworks' not in main_commands:
    raise SystemExit('App loader lacks the bundled library search path')
print(f'ClientMod device bundle: {len(binaries)} ARM64 binaries; original factories/launcher and dependencies PASS')
print('This checks packaging, not on-device graphical initialization or gameplay.')
