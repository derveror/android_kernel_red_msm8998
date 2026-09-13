from __future__ import annotations

import subprocess
import tempfile
import textwrap
import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]


class DmAndroidVerityPolicyTest(unittest.TestCase):
    def test_only_eng_or_bootloader_unlocked_devices_bypass_a_missing_key(self) -> None:
        source = textwrap.dedent(
            """
            #include "drivers/md/dm-android-verity-policy.h"

            int main(void)
            {
                if (dm_android_verity_allow_linear_without_key(0, 0))
                    return 1;
                if (!dm_android_verity_allow_linear_without_key(1, 0))
                    return 2;
                if (!dm_android_verity_allow_linear_without_key(0, 1))
                    return 3;
                if (!dm_android_verity_allow_linear_without_key(1, 1))
                    return 4;
                return 0;
            }
            """
        )

        with tempfile.TemporaryDirectory(prefix="dm-android-verity-policy-") as tmp:
            executable = Path(tmp) / "policy-test"
            compiled = subprocess.run(
                [
                    "cc",
                    "-std=c99",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(KERNEL_ROOT),
                    "-x",
                    "c",
                    "-o",
                    str(executable),
                    "-",
                ],
                input=source,
                text=True,
                capture_output=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)

            result = subprocess.run([str(executable)], check=False)
            self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
