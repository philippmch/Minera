"""Check the transport oracle rejects semantic differences and precision loss."""

import copy
import gzip
import json
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECKER = runpy.run_path(str(ROOT / "scripts/check-format-dataframe-transports.py"))
REPORT = json.loads(
    gzip.decompress((ROOT / "docs/format-dataframe-reference.json.gz").read_bytes())
)


class FormatterTransportTests(unittest.TestCase):
    def response(self, row):
        result = {
            "family": "FORMAT_DATAFRAME",
            "input_after": CHECKER["wire_table"](row["input_after"]),
        }
        if "error" in row:
            result["error"] = row["error"]["message"]
        else:
            result["table"] = CHECKER["wire_table"](row["result"])
        return result

    def test_all_representable_observations_map(self):
        included = 0
        for row in REPORT["rows"]:
            if row["name"] == "non_string_column_labels_with_selectors":
                continue
            CHECKER["compare"](row, self.response(row))
            included += 1
        self.assertEqual(included, 190)

    def test_scalar_precision_kind_and_projection_differences_reject(self):
        row = next(
            row for row in REPORT["rows"] if row["name"] == "object_unsigned_timestamps"
        )
        for change in (
            lambda cell: cell.update(typed_value=float(cell["typed_value"])),
            lambda cell: cell.update(value="1"),
            lambda cell: cell.update(kind="float"),
            lambda cell: cell.update(key="wrong"),
        ):
            result = self.response(row)
            change(result["table"]["rows"][0][2])
            with self.assertRaises(ValueError):
                CHECKER["compare"](row, result)

    def test_dtype_index_category_and_attrs_differences_reject(self):
        row = next(
            row
            for row in REPORT["rows"]
            if row["name"] == "custom_selectors_and_existing_start"
        )
        for change in (
            lambda table: table["dtypes"].__setitem__(0, "object"),
            lambda table: table["index"].update(names=["changed"]),
            lambda table: table["categorical_dtypes"].__setitem__(0, None),
            lambda table: table["metadata"].update(properties=[]),
        ):
            result = self.response(row)
            change(result["table"])
            with self.assertRaises(ValueError):
                CHECKER["compare"](row, result)

    def test_rejection_requires_post_call_mutation(self):
        row = next(
            row
            for row in REPORT["rows"]
            if row["name"] == "numeric_boundary_custom_error_mutation"
        )
        result = self.response(row)
        result["input_after"] = CHECKER["wire_table"](row["input"])
        with self.assertRaises(ValueError):
            CHECKER["compare"](row, result)
        result = self.response(row)
        result["table"] = copy.deepcopy(result["input_after"])
        with self.assertRaises(ValueError):
            CHECKER["compare"](row, result)


if __name__ == "__main__":
    unittest.main()
