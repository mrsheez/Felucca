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
            parts = sec.split(".")                   # (-ffunction-sections: .text.name -> .text)
            syms.append((size, "." + parts[1] if len(parts) > 2 and not sec.startswith(".ram") else sec, name))
    tot = {}
    for size, sec, _ in syms:
        tot[sec] = tot.get(sec, 0) + size
    for sec, size in sorted(tot.items(), key=lambda kv: -kv[1]):
        print(f"{size:9d}  {sec}  (all symbols)")
    print()
    groups = {}
    for size, sec, name in syms:                     # EDDA OS's parts by name (a rough split; the rest is Felucca's)
        g = next((p for p in ("edda", "vz_", "arv", "ARV", "EDDA", "ED_", "ed_", "kit", "KIT", "smp", "SMP", "font", "AF_",
                              "icon", "ICON", "fm6", "FM6", "uac", "usb", "con_", "ui_", "gfx", "lcd") if name.startswith(p)), "other")
        groups[g] = groups.get(g, 0) + size
    for g, size in sorted(groups.items(), key=lambda kv: -kv[1]):
        print(f"{size:9d}  [{g}]")
    print()
    for size, sec, name in sorted(syms, reverse=True)[:n]:
        print(f"{size:9d}  {sec:10s}  {name}")


if __name__ == "__main__":
    main()
