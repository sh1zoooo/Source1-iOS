#!/usr/bin/env python3
"""Bound the pinned SDL UIKit pump without changing the checked-out dependency."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("sdl", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
source = args.sdl.resolve() / "src/video/uikit/SDL_uikitevents.m"
text = source.read_text()
old = "    SInt32 result;\n    do {"
new = """    SInt32 result;
    unsigned iterations = 0;
    const CFAbsoluteTime deadline = CFAbsoluteTimeGetCurrent() + 0.004;
    do {
        ++iterations;"""
if text.count(old) != 1:
    raise RuntimeError("SDL UIKit default-mode pump context changed")
text = text.replace(old, new)
old = "    } while (result == kCFRunLoopRunHandledSource);"
if text.count(old) != 1:
    raise RuntimeError("SDL UIKit default-mode loop context changed")
text = text.replace(old, "    } while (result == kCFRunLoopRunHandledSource && iterations < 64 && CFAbsoluteTimeGetCurrent() < deadline);")
old = "    } while(result == kCFRunLoopRunHandledSource);"
if text.count(old) != 1:
    raise RuntimeError("SDL UIKit tracking-mode loop context changed")
text = text.replace("    /* Make sure UIScrollView objects scroll properly. */\n    do {",
                    "    /* Make sure UIScrollView objects scroll properly. */\n    iterations = 0;\n    do {\n        ++iterations;")
text = text.replace(old, "    } while(result == kCFRunLoopRunHandledSource && iterations < 64 && CFAbsoluteTimeGetCurrent() < deadline);")
# Retain the pinned SDL headers when compiling this generated copy elsewhere.
text = re.sub(r'#include "([^"]+)"',
              lambda m: '#include "' + str((source.parent / m[1]).resolve()) + '"', text)
args.output.parent.mkdir(parents=True, exist_ok=True)
if not args.output.exists() or args.output.read_text() != text:
    args.output.write_text(text)
