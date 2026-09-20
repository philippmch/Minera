"""Evidence gates for typed-date formatter precision."""

import copy
import gzip
from pathlib import Path
import runpy
import unittest

ROOT = Path(__file__).resolve().parents[2]
CHECK = runpy.run_path(str(ROOT / "scripts/check-formatter-date-resolution.py"))
RAW = gzip.decompress(
    (ROOT / "docs/formatter-date-resolution-reference.json.gz").read_bytes()
)
REPORT = CHECK["load"](RAW)


class FormatterDateResolutionEvidence(unittest.TestCase):
    def test_manifest_and_provenance(self):
        self.assertEqual(len(REPORT["rows"]), 21)
        self.assertEqual(REPORT["reference_version"], "2.7.22")
        self.assertEqual(REPORT["pandas_version"], "3.0.5")
        proof = REPORT["reference_provenance"]
        self.assertEqual(proof["commit"], "0715c31f3f1d85924107d6aa5329806db69df899")
        self.assertEqual(proof["installed_sha256"], proof["pinned_sha256"])
        self.assertEqual(proof["function"], "format_dataframe")
        for row in REPORT["rows"]:
            self.assertNotIn("error", row)
        with self.assertRaises(ValueError):
            CHECK["load"](RAW + b" ")

    def test_public_units_and_start_projection(self):
        for row in REPORT["rows"]:
            dtypes = row["result"]["dtypes"]
            name = row["name"]
            unit = "us" if name.endswith("_us") or name.endswith("start_ns") else "ns"
            self.assertEqual(dtypes["time:timestamp"], "datetime64[" + unit + ", UTC]")
            if name.endswith("start_ns"):
                self.assertEqual(dtypes["start_timestamp"], "datetime64[ns, UTC]")

    def test_comparison_rejects_metadata_loss(self):
        for row in REPORT["rows"]:
            actual = {
                "family": "FORMAT_DATAFRAME",
                "input_after": CHECK["wire"](row["input_after"]),
                "table": CHECK["wire"](row["result"]),
            }
            CHECK["compare"](row, actual)
            damaged_timezone = copy.deepcopy(actual)
            damaged_timezone["table"]["rows"][0][2].pop("date_timezone")
            with self.assertRaisesRegex(ValueError, "Date timezone metadata differs"):
                CHECK["compare"](row, damaged_timezone)
            if row["name"].endswith("object_ns"):
                damaged = copy.deepcopy(actual)
                damaged["table"]["rows"][0][2]["date_resolution"] = "us"
                with self.assertRaisesRegex(ValueError, "Date resolution differs"):
                    CHECK["compare"](row, damaged)


if __name__ == "__main__":
    unittest.main()
