# Copyright (C) 2026 Nithin Aashik
# SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial

"""Checks that every source file starts with its copyright and license header, or adds it.

    python tools/license_headers.py               checks every tracked source file
    python tools/license_headers.py --fix         adds the header where it's missing
    python tools/license_headers.py FILE...       checks (or fixes) just those files

The header names both licenses Gazeora is offered under (COMMERCIAL-LICENSE.md):

    // Copyright (C) 2026 Nithin Aashik
    // SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-Gazeora-Commercial
"""

import argparse
import pathlib
import re
import subprocess
import sys

COPYRIGHT = "Copyright (C) 2026 Nithin Aashik"
LICENSE = "GPL-3.0-only OR LicenseRef-Gazeora-Commercial"
SPDX = re.compile(r"SPDX-License-Identifier: ")
# How far into a file the header may sit, after a shebang or the like
HEAD_LINES = 6

# Each kind of file's comment: its opening and closing
SLASHES = ("// ", "")
HASHES = ("# ", "")
XML = ("<!-- ", " -->")
STYLES = {
    ".c": SLASHES, ".cpp": SLASHES, ".h": SLASHES, ".rc": SLASHES, ".java": SLASHES,
    ".kt": SLASHES, ".kts": SLASHES, ".vert": SLASHES, ".frag": SLASHES,
    ".py": HASHES, ".sh": HASHES, ".cmake": HASHES,
    ".wxs": XML, ".wxi": XML,
}
NAMED = {"CMakeLists.txt": HASHES}


def style_of(path):
    return NAMED.get(path.name) or STYLES.get(path.suffix)


def header(style, license_expression=LICENSE):
    opening, closing = style
    return [f"{opening}{COPYRIGHT}{closing}",
            f"{opening}SPDX-License-Identifier: {license_expression}{closing}"]


def has_header(lines):
    head = lines[:HEAD_LINES]
    return any(COPYRIGHT in line for line in head) and any(SPDX.search(line) for line in head)


def add_header(path, style):
    raw = path.read_bytes().decode("utf-8")
    newline = "\r\n" if "\r\n" in raw else "\n"
    lines = raw.split(newline)
    at = 1 if lines and lines[0].startswith("#!") else 0
    lines[at:at] = header(style) + [""]
    path.write_bytes(newline.join(lines).encode("utf-8"))


def tracked_files():
    listed = subprocess.run(["git", "ls-files"], capture_output=True, text=True, check=True)
    return [pathlib.Path(name) for name in listed.stdout.splitlines()]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--fix", action="store_true", help="add the header where it's missing")
    parser.add_argument("files", nargs="*", type=pathlib.Path)
    args = parser.parse_args()

    missing = []
    for path in args.files or tracked_files():
        style = style_of(path)
        if style is None or not path.is_file():
            continue
        if has_header(path.read_text(encoding="utf-8").splitlines()):
            continue
        if args.fix:
            add_header(path, style)
        missing.append(path)

    for path in missing:
        print(f"{'added the license header to' if args.fix else 'no license header:'} {path}")
    if missing and not args.fix:
        print("Run `python tools/license_headers.py --fix` to add it.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
