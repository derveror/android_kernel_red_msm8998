from __future__ import annotations

import subprocess
import tempfile
import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
DEFCONFIG = "lineageos_hydrogenone_defconfig"


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


class HydrogenOneDefconfigContractTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.config = resolved_hydrogenone_config()

    def test_qca_cld_wlan_driver_is_builtin(self) -> None:
        self.assertEqual(
            self.config.get("CONFIG_QCA_CLD_WLAN"),
            "y",
            "the RED 4.4 module loader corrupts qcacld symbol CRCs after KASLR relocation",
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
