# AC79 SDK tool files (vendored)

`cpu/wl82/tools/uboot.boot`, `cpu/wl82/tools/cfg_tool.bin` and `cpu/wl82/tools/cfg/eq_cfg_hw.bin` from JieLi's
fw-AC79_AIoT_SDK (Apache-2.0, LICENSE beside this file), taken from a GitHub mirror of the SDK tree
(tuya/TuyaOpen-JieLi, chip/wl82/AC79_AIoT_SDK) on 7 Oct 2026. Felucca's build.py reads them from `$AC79_SDK`.
`uboot.boot` and `cfg/eq_cfg_hw.bin` differ from the tag Felucca's BUILDING.md names (AC79NN_SDK_V1.2.1_2023-12-13;
build.py warns), so `edda-build.sh` and the CI first fetch that tag's three files with `tools/get_sdk_files.sh`
(checked against build.py's SHA-256s) and fall back to these only when gitee cannot be reached.
