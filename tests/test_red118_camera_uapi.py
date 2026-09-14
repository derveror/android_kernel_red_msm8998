from __future__ import annotations

import re
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HEADERS = (
    "include/uapi/media/msm_camsensor_sdk.h",
    "include/uapi/media/ais/msm_ais_sensor_sdk.h",
)


class Red118CameraUapiTest(unittest.TestCase):
    def test_red118_power_array_capacity(self) -> None:
        failures = []
        for relative in HEADERS:
            text = (ROOT / relative).read_text(encoding="utf-8")
            match = re.search(r"^#define\s+MAX_POWER_CONFIG\s+(\d+)\s*$", text, re.MULTILINE)
            if match is None:
                failures.append(f"{relative}: MAX_POWER_CONFIG is not defined")
            elif int(match.group(1)) != 16:
                failures.append(f"{relative}: MAX_POWER_CONFIG={match.group(1)}, expected 16")
        self.assertEqual(failures, [], "RED .118 camera UAPI mismatch:\n" + "\n".join(failures))

    def test_power_setting_arrays_remain_bound_to_the_constant(self) -> None:
        for relative in HEADERS:
            text = (ROOT / relative).read_text(encoding="utf-8")
            self.assertIn("power_setting_a[MAX_POWER_CONFIG]", text, relative)
            self.assertIn("power_down_setting_a[MAX_POWER_CONFIG]", text, relative)


if __name__ == "__main__":
    unittest.main()
