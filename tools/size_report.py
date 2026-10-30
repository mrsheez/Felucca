#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: the largest symbols of a build, from `objdump -t felucca.elf` on stdin (the CI keeps it on the ci-out
branch, so a build that does not fit the app slot says where the bytes are without a toolchain at home).
  objdump -t build/felucca.elf | python3 tools/size_report.py [N]
Prints the totals per section, then the N (default 150) largest symbols: size, section, name."""
import re
import sys

LINE = re.compile(r"^([0-9a-f]+)\s.{7}\s(\S+)\s+([0-9a-f]+)\s+(\S+)\s*$")


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 150
    syms = []
    for ln in sys.stdin:
        m = LINE.match(ln.rstrip("\n"))
        if not m:
            continue
        sec, size, name = m.group(2), int(m.group(3), 16), m.group(4)
        if size and not sec.startswith("*"):
            syms.append((size, sec, name))
    tot = {}
    for size, sec, _ in syms:
        tot[sec] = tot.get(sec, 0) + size
    for sec, size in sorted(tot.items(), key=lambda kv: -kv[1]):
        print(f"{size:9d}  {sec}  (all symbols)")
    print()
    for size, sec, name in sorted(syms, reverse=True)[:n]:
        print(f"{size:9d}  {sec:10s}  {name}")


if __name__ == "__main__":
    main()
