#!/usr/bin/env python3
"""Import mobile resources from a user-provided ClientMod APK into an iOS cache.
Usage: python3 tools/import_android_extras.py CLIENTMOD.apk CONTENT/cm
Android native libraries are deliberately not copied: they cannot run on iOS.
"""
import argparse
import pathlib
import struct
import zipfile


def import_extras(apk, destination):
    with zipfile.ZipFile(apk) as package:
        info = package.getinfo('assets/extras_dir.vpk')
        if info.file_size > 64 * 1024 * 1024:
            raise ValueError('extras archive exceeds 64 MiB')
        data = package.read(info)  # ZIP CRC is verified by zipfile.
    signature, version, tree_size = struct.unpack_from('<III', data)
    if signature != 0x55AA1234 or version not in (1, 2):
        raise ValueError('unsupported VPK header')
    header_size = 12 if version == 1 else 28
    tree_end = header_size + tree_size
    if tree_end > len(data):
        raise ValueError('truncated VPK tree')
    cursor = header_size

    def string():
        nonlocal cursor
        end = data.index(b'\0', cursor, tree_end)
        value = data[cursor:end].decode('utf-8')
        cursor = end + 1
        return value

    config = None
    while True:
        extension = string()
        if not extension:
            break
        while True:
            directory = string()
            if not directory:
                break
            while True:
                name = string()
                if not name:
                    break
                if cursor + 18 > tree_end:
                    raise ValueError('truncated VPK entry')
                crc, preload, archive, offset, size, terminator = struct.unpack_from('<IHHIIH', data, cursor)
                cursor += 18
                if terminator != 0xFFFF or cursor + preload > tree_end:
                    raise ValueError('invalid VPK entry')
                prefix = data[cursor:cursor + preload]
                cursor += preload
                if archive != 0x7FFF or tree_end + offset + size > len(data):
                    raise ValueError('extras must be a self-contained VPK')
                if directory == 'cfg' and name == 'touch_default' and extension == 'cfg':
                    config = prefix + data[tree_end + offset:tree_end + offset + size]
    if config is None or len(config) > 256 * 1024:
        raise ValueError('mobile default touch configuration missing')
    destination = pathlib.Path(destination)
    destination.mkdir(parents=True, exist_ok=True)
    (destination / 'extras_dir.vpk').write_bytes(data)
    (destination / 'cfg').mkdir(exist_ok=True)
    (destination / 'cfg' / 'touch_apk.cfg').write_bytes(config)
    print('Imported extras_dir.vpk and cfg/touch_apk.cfg into', destination)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('apk', type=pathlib.Path)
    parser.add_argument('destination', type=pathlib.Path)
    args = parser.parse_args()
    import_extras(args.apk, args.destination)
