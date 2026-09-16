from __future__ import annotations

import re
import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
SPINLOCK_DEBUG = KERNEL_ROOT / "kernel/locking/spinlock_debug.c"


class SpinlockCrashDiagnosticsTest(unittest.TestCase):
    def test_watchdog_bite_branch_dumps_stack_before_reset(self) -> None:
        source = SPINLOCK_DEBUG.read_text(encoding="utf-8")
        match = re.search(
            r"static void spin_dump\([^)]*\)\s*\{(?P<body>.*?)\n\}",
            source,
            re.DOTALL,
        )
        self.assertIsNotNone(match, "spin_dump() is missing")
        if match is None:
            return

        body = match.group("body")
        bite_match = re.search(
            r"#ifdef CONFIG_DEBUG_SPINLOCK_BITE_ON_BUG"
            r"(?P<bite>.*?)"
            r"#elif defined\(CONFIG_DEBUG_SPINLOCK_PANIC_ON_BUG\)",
            body,
            re.DOTALL,
        )
        self.assertIsNotNone(bite_match, "watchdog-bite diagnostics branch is missing")
        if bite_match is None:
            return

        bite = bite_match.group("bite")
        self.assertIn("dump_stack();", bite)
        self.assertIn("msm_trigger_wdog_bite();", bite)
        self.assertLess(
            bite.index("dump_stack();"),
            bite.index("msm_trigger_wdog_bite();"),
            "the watchdog reset must not erase the call stack needed to diagnose "
            "spinlock corruption",
        )


if __name__ == "__main__":
    unittest.main()
