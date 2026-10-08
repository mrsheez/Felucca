#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# EDDA OS: the three AC79 SDK files a package carries (uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin; JieLi,
# Apache-2.0) from JieLi's own tag, the one Felucca's BUILDING.md names, checked against tools/build.py's SHA-256s.
#   sh tools/get_sdk_files.sh DIR     -> DIR/cpu/wl82/tools/...; exit 0 when all three match the reference, 1 when not
#                                        (then use third_party/ac79-sdk-tools: two of those differ from it)
# The raw files first (three small downloads), else a shallow sparse fetch of the tag. DIR kept: a second run with the
# files in place downloads nothing.
set -u
DIR="$1"
TAG=AC79NN_SDK_V1.2.1_2023-12-13
URL=https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK
FILES="uboot.boot cfg_tool.bin cfg/eq_cfg_hw.bin"
cd "$(dirname "$0")/.." || exit 1

check() {
    python3 - "$DIR" <<'PY'
import hashlib, re, sys
src = open("tools/build.py").read()
want = dict(re.findall(r'"([\w./]+)": "([0-9a-f]{64})"', src[src.index("SDK_SHA256"):src.index("}", src.index("SDK_SHA256"))]))
bad = []
for f, h in want.items():
    try:
        got = hashlib.sha256(open(f"{sys.argv[1]}/cpu/wl82/tools/{f}", "rb").read()).hexdigest()
    except OSError:
        got = None
    if got != h:
        bad.append(f)
print("SDK files: " + ("JieLi's reference, all three" if not bad else "not the reference: " + " ".join(bad)))
sys.exit(1 if bad or len(want) != 3 else 0)
PY
}

mkdir -p "$DIR/cpu/wl82/tools/cfg"
check >/dev/null 2>&1 && { check; exit 0; }
for f in $FILES; do
    curl -fsSL --retry 2 --max-time 120 -o "$DIR/cpu/wl82/tools/$f" "$URL/raw/$TAG/cpu/wl82/tools/$f" 2>/dev/null || true
done
check >/dev/null 2>&1 && { check; exit 0; }
G="$DIR/.src"                                    # (the tag, its tree without the blobs, those three checked out)
rm -rf "$G"
git init -q "$G" && git -C "$G" remote add origin "$URL.git" && git -C "$G" config core.sparseCheckout true &&
    printf '/cpu/wl82/tools/uboot.boot\n/cpu/wl82/tools/cfg_tool.bin\n/cpu/wl82/tools/cfg/eq_cfg_hw.bin\n' \
        > "$G/.git/info/sparse-checkout" &&
    { timeout 1500 git -C "$G" fetch -q --depth 1 --filter=blob:none origin "+refs/tags/$TAG:refs/tags/$TAG" 2>/dev/null ||
      timeout 1500 git -C "$G" fetch -q --depth 1 origin "+refs/tags/$TAG:refs/tags/$TAG"; } &&
    git -C "$G" checkout -q "$TAG" &&
    for f in $FILES; do cp "$G/cpu/wl82/tools/$f" "$DIR/cpu/wl82/tools/$f"; done
rm -rf "$G"
check
