#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# EDDA OS: build the installable package (build/felucca.fwsc) in one step.
#   sh edda-build.sh            on Linux x86-64 (or macOS with Docker: Felucca's build.py runs the tools in a
#                               container when the host cannot)
# Needs: python3 with Pillow and fontTools, curl, tar, xz; network to pkgman.jieliapp.com for JieLi's toolchain.
# The three AC79 SDK files the package needs (uboot.boot, cfg_tool.bin, cfg/eq_cfg_hw.bin) ship in
# third_party/ac79-sdk-tools (JieLi, Apache-2.0: its LICENSE is beside them), so the SDK clone is optional.
set -e
cd "$(dirname "$0")"
TC="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
if [ ! -x "$TC/pi32v2/bin/clang" ]; then
    echo "== JieLi pi32v2 toolchain -> $HOME/.jieli"
    sh tools/get_toolchain.sh "$HOME/.jieli"
    TC="$HOME/.jieli/toolchain"
fi
export JIELI_TOOLCHAIN="$TC"
export AC79_SDK="${AC79_SDK:-$PWD/third_party/ac79-sdk-tools}"
python3 -c "import PIL, fontTools" 2>/dev/null || pip3 install --user Pillow fonttools
echo "== build (JIELI_TOOLCHAIN=$JIELI_TOOLCHAIN, AC79_SDK=$AC79_SDK)"
./build.sh "$@"
ls -la build/felucca.fwsc
echo "== install: python3 tools/fm1_install.py build/felucca.fwsc   (or the web installer: see EDDA-OS.md 4)"
