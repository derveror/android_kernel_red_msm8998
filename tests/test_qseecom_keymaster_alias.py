from __future__ import annotations

import unittest
from pathlib import Path


KERNEL_ROOT = Path(__file__).resolve().parents[1]
QSEECOM = KERNEL_ROOT / "drivers/misc/qseecom.c"


class QseecomKeymasterAliasTest(unittest.TestCase):
    def test_appsbl_keymaster_alias_retries_the_loaded_64_bit_name(self) -> None:
        source = QSEECOM.read_text(encoding="utf-8")
        function_start = source.index("static int qseecom_query_app_loaded(")
        function_end = source.index(
            "static int __qseecom_get_ce_pipe_info(", function_start
        )
        function = source[function_start:function_end]

        initial_lookup = function.index(
            "ret = __qseecom_check_app_exists(req, &app_id);"
        )
        alias_guard = function.index(
            "if (!app_id && qseecom.is_apps_region_protected &&\n"
            "\t\t!strcmp(query_req.app_name, \"keymaster\"))"
        )
        alias_name = function.index(
            'strlcpy(req.app_name, "keymaster64", MAX_APP_NAME_SIZE);'
        )
        alias_lookup = function.index(
            "ret = __qseecom_check_app_exists(req, &app_id);",
            initial_lookup + 1,
        )
        loaded_branch = function.index("if (app_id) {")

        self.assertLess(initial_lookup, alias_guard)
        self.assertLess(alias_guard, alias_name)
        self.assertLess(alias_name, alias_lookup)
        self.assertLess(alias_lookup, loaded_branch)


if __name__ == "__main__":
    unittest.main()
