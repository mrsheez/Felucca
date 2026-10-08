# AC79 SDK tool files (vendored)

`cpu/wl82/tools/uboot.boot`, `cpu/wl82/tools/cfg_tool.bin` and `cpu/wl82/tools/cfg/eq_cfg_hw.bin` from JieLi's
fw-AC79_AIoT_SDK (Apache-2.0, LICENSE beside this file), taken from a GitHub mirror of the SDK tree
(tuya/TuyaOpen-JieLi, chip/wl82/AC79_AIoT_SDK) on 7 Oct 2026. Felucca's build.py reads them from `$AC79_SDK`;
`edda-build.sh` points `AC79_SDK` here so the gitee clone is not needed. Replace them with the files from the tag
Felucca's BUILDING.md names (AC79NN_SDK_V1.2.1_2023-12-13) if a package built with these does not boot.
