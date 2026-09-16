from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
DEFCONFIG = "lineageos_hydrogenone_defconfig"
MODULE_SOURCE = KERNEL_ROOT / "kernel/module.c"


def resolved_hydrogenone_config() -> dict[str, str]:
    with tempfile.TemporaryDirectory(prefix="hydrogenone-kconfig-") as output:
        subprocess.run(
            [
                "make",
                "--silent",
                "--no-print-directory",
                "-C",
                str(KERNEL_ROOT),
                f"O={output}",
                "ARCH=arm64",
                DEFCONFIG,
            ],
            check=True,
        )
        values: dict[str, str] = {}
        for line in (Path(output) / ".config").read_text(encoding="utf-8").splitlines():
            if line.startswith("CONFIG_") and "=" in line:
                key, value = line.split("=", 1)
                values[key] = value
        return values


def preprocessed_arm64_module_macros() -> dict[str, str]:
    result = subprocess.run(
        [
            "cc",
            "-dM",
            "-E",
            "-x",
            "c",
            "-DCONFIG_RANDOMIZE_BASE",
            "-DCONFIG_MODVERSIONS",
            "-D__ASM_MEMORY_H",
            f"-I{KERNEL_ROOT / 'arch/arm64/include'}",
            f"-I{KERNEL_ROOT / 'include'}",
            "-",
        ],
        input="#include <asm/module.h>\n",
        text=True,
        capture_output=True,
        check=True,
    )
    macros: dict[str, str] = {}
    for line in result.stdout.splitlines():
        fields = line.split(maxsplit=2)
        if len(fields) >= 2 and fields[0] == "#define":
            macros[fields[1]] = fields[2] if len(fields) == 3 else ""
    return macros


class HydrogenOneDefconfigContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.config = resolved_hydrogenone_config()

    def test_large_wlan_driver_is_a_loadable_module(self) -> None:
        self.assertEqual(
            self.config.get("CONFIG_QCA_CLD_WLAN"),
            "m",
            "built-in qcacld exceeds the RED bootloader's 16 MiB kernel payload window",
        )

    def test_arm64_unapplies_kaslr_from_kernel_symbol_crcs(self) -> None:
        macros = preprocessed_arm64_module_macros()
        self.assertIn(
            "ARCH_RELOCATES_KCRCTAB",
            macros,
            "the module loader must remove the ARM64 KASLR delta from kernel CRCs",
        )
        self.assertEqual(
            macros.get("reloc_start"),
            "(kimage_vaddr - KIMAGE_VADDR)",
            "kernel symbol CRCs are relocated by the kernel image KASLR delta",
        )

    def test_module_loader_accepts_raw_or_kaslr_relocated_kernel_crcs(self) -> None:
        source = MODULE_SOURCE.read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r"if \(versions\[i\]\.crc == \*crc \|\|\s*"
            r"versions\[i\]\.crc == maybe_relocated\(\*crc, crc_owner\)\)",
            "Clang/LLD can leave absolute CRCs unrelocated while GNU-style "
            "links require the ARM64 KASLR adjustment; the loader must accept both",
        )

    def test_red_runtime_drivers_remain_builtin(self) -> None:
        required = {
            "CONFIG_BACKLIGHT_LM36923H": "y",
            "CONFIG_INPUT_FPC_FINGERPRINT": "y",
            "CONFIG_TOUCHSCREEN_CYPRESS_CYTTSP5": "y",
            "CONFIG_SND_SOC_TFA9894": "y",
        }
        actual = {name: self.config.get(name) for name in required}
        self.assertEqual(actual, required)


if __name__ == "__main__":
    unittest.main()
