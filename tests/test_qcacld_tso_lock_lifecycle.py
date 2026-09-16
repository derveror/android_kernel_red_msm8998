from __future__ import annotations

import re
import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
OL_TXRX = KERNEL_ROOT / "drivers/staging/qcacld-3.0/core/dp/txrx/ol_txrx.c"


class QcacldTsoLockLifecycleTest(unittest.TestCase):
    def test_tso_lock_lifecycle_exists_without_tso_debug_stats(self) -> None:
        source = OL_TXRX.read_text(encoding="utf-8")
        non_debug = re.search(
            r"#else\s+"
            r"static void ol_txrx_stats_display_tso\([^)]*\)\s*\{.*?\}\s+"
            r"static void ol_txrx_tso_stats_init\([^)]*\)\s*"
            r"\{(?P<init>.*?)\}\s+"
            r"static void ol_txrx_tso_stats_deinit\([^)]*\)\s*"
            r"\{(?P<deinit>.*?)\}\s+"
            r"static void ol_txrx_tso_stats_clear",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(
            non_debug,
            "the non-FEATURE_TSO_DEBUG lifecycle implementation is missing",
        )
        if non_debug is None:
            return

        init = non_debug.group("init")
        deinit = non_debug.group("deinit")

        self.assertIn("#if defined(FEATURE_TSO)", init)
        self.assertIn(
            "qdf_spinlock_create(&pdev->stats.pub.tx.tso.tso_stats_lock);",
            init,
        )
        self.assertIn("#if defined(FEATURE_TSO)", deinit)
        self.assertIn(
            "qdf_spinlock_destroy(&pdev->stats.pub.tx.tso.tso_stats_lock);",
            deinit,
        )


if __name__ == "__main__":
    unittest.main()
