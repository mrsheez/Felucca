#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# EDDA OS: the image's size under build options (CI, after a build that did not fit the app slot): each line the
# options and the linker's verdict (how far over) or build.py's size line. Needs the toolchain (tools/build.py).
#   sh tools/size_matrix.sh VERSION > build-matrix.txt
V="${1:-1.7.0-EDDA}"
try() {
    log=$(env "$@" python3 tools/build.py --release "$V" 2>&1)
    r=$(printf '%s\n' "$log" | grep -oE "overflowed by [0-9]+ bytes|image [0-9]+ B; RAM .*|exceeds the app slot|pool headroom [0-9]+ B < [0-9]+ B" | tail -2 | tr '\n' ' ')
    echo "$* : ${r:-$(printf '%s\n' "$log" | tail -1)}"
}
try EDDA_VIZ=1
try EDDA_VIZ=0
try EDDA_VIZ=0 FELUCCA_CDC=0
try EDDA_VIZ=0 FELUCCA_CPU2=0
try EDDA_VIZ=0 FELUCCA_CDC=0 FELUCCA_CPU2=0
try EDDA_VIZ=0 FELUCCA_UAC=0
try EDDA_VIZ=0 FELUCCA_UAC_48K=0
try EDDA_VIZ=0 FELUCCA_UAC_48K=0 FELUCCA_CDC=0
try EDDA_VIZ=0 FELUCCA_UAC_48K=0 FELUCCA_CDC=0 FELUCCA_CPU2=0
try EDDA_VIZ=1 FELUCCA_CDC=0 FELUCCA_CPU2=0
try EDDA_VIZ=1 FELUCCA_UAC_48K=0 FELUCCA_CDC=0 FELUCCA_CPU2=0
try EDDA_VIZ=1 FELUCCA_CDC=0 FELUCCA_CPU2=0 FELUCCA_UAC=0
