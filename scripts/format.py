#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
FILES = [
    path
    for folder in ("src", "tests")
    for path in sorted((ROOT / folder).rglob("*"))
    if path.suffix in (".cpp", ".h", ".mm")
]

check = "--check" in sys.argv
changed = []
for path in FILES:
    current = path.read_bytes()
    wanted = subprocess.run(
        ["clang-format", str(path)], cwd=ROOT, capture_output=True, check=True
    ).stdout

    if wanted == current:
        continue

    changed.append(path.relative_to(ROOT))

    if not check:
        path.write_bytes(wanted)

verb = "would reformat" if check else "reformatted"

for path in changed:
    print(f"{verb}: {path}")

print(f"{len(FILES)} files checked, {len(changed)} {verb}")

sys.exit(1 if check and changed else 0)
