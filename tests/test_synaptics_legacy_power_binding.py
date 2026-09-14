from __future__ import annotations

import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
SYNAPTICS_CORE = (
    KERNEL_ROOT
    / "drivers/input/touchscreen/synaptics_dsx/synaptics_dsx_core.c"
)


class SynapticsLegacyPowerBindingTest(unittest.TestCase):
    def test_legacy_bus_supply_bypasses_the_strict_dual_rail_contract(self) -> None:
        source = SYNAPTICS_CORE.read_text(encoding="utf-8")
        configure_start = source.index(
            "static int synaptics_dsx_regulator_configure("
        )
        configure_end = source.index(
            "static int synaptics_dsx_regulator_enable(", configure_start
        )
        configure = source[configure_start:configure_end]

        legacy_property = configure.find('"synaptics,bus-reg-name"')
        strict_vdd_current = configure.index('"synaptics,vdd-current"')
        self.assertGreaterEqual(legacy_property, 0)

        legacy_return = configure.index("return 0;", legacy_property)

        self.assertLess(legacy_property, legacy_return)
        self.assertLess(legacy_return, strict_vdd_current)
        self.assertIn(
            "regulator_get(rmi4_data->pdev->dev.parent,\n"
            "\t\t\tlegacy_regulator_name)",
            configure[legacy_property:legacy_return],
        )
        self.assertIn(
            "rmi4_data->regulator_avdd = NULL;",
            configure[legacy_property:legacy_return],
        )

    def test_optional_legacy_avdd_is_guarded_during_power_transitions(self) -> None:
        source = SYNAPTICS_CORE.read_text(encoding="utf-8")
        enable_start = source.index("static int synaptics_dsx_regulator_enable(")
        enable_end = source.index("synaptics_rmi4_probe()", enable_start)
        enable = source[enable_start:enable_end]

        self.assertIn(
            "if (rmi4_data->regulator_avdd) {\n"
            "\t\t\tretval = regulator_enable(rmi4_data->regulator_avdd);",
            enable,
        )
        self.assertIn(
            "if (rmi4_data->regulator_avdd)\n"
            "\t\t\tregulator_disable(rmi4_data->regulator_avdd);",
            enable,
        )

    def test_probe_cleanup_accepts_a_single_legacy_supply(self) -> None:
        source = SYNAPTICS_CORE.read_text(encoding="utf-8")
        probe_start = source.index("static int synaptics_rmi4_probe(")
        probe_end = source.index("static int synaptics_rmi4_remove(", probe_start)
        cleanup_start = source.index("err_set_gpio:", probe_start, probe_end)
        cleanup = source[cleanup_start:probe_end]

        self.assertIn(
            "if (rmi4_data->regulator_vdd) {\n"
            "\t\tregulator_disable(rmi4_data->regulator_vdd);",
            cleanup,
        )
        self.assertIn(
            "if (rmi4_data->regulator_avdd) {\n"
            "\t\tregulator_disable(rmi4_data->regulator_avdd);",
            cleanup,
        )


if __name__ == "__main__":
    unittest.main()
