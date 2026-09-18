"""Offline manifest/provenance checks for the public unsigned XES boundaries."""

import gzip
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
REPORT = json.loads(
    gzip.decompress((ROOT / "docs/xes-unsigned-reference.json.gz").read_bytes())
)
PINNED_MODULES = {
    "pm4py.objects.log.importer.xes.importer": "0268e972f9e17aebfcea6eeb26be784d6b02af06731f0a1441dbe0df674dd603",
    "pm4py.objects.log.importer.xes.variants.iterparse": "b4c6e8857936a75066faa4dbedb9aa3f379591880502298adc3abb6ed32b7a8b",
    "pm4py.objects.log.importer.xes.variants.line_by_line": "e456edf71b2db82299adc7f8d166c403edc852144fe0356903bf39fd40744d9f",
    "pm4py.objects.log.importer.xes.variants.iterparse_mem_compressed": "9a2599eaa13e92f2f38f67fb932aee66baa62a233b8a260091f98625ea607033",
    "pm4py.objects.log.importer.xes.variants.iterparse_20": "4425e1ab1397909dd4c3b7cebcf98fe12428e6e8142fcfb18962ca1738a159cb",
    "pm4py.objects.log.importer.xes.variants.chunk_regex": "8b47d844216e1a640badc410387c7f81409a793bff3bf48c47cf8bff2a79a03d",
    "pm4py.objects.log.importer.xes.variants.rustxes": "076b324dc861af5c0c88ed9c6012215fa91aab238999262adbbdb2d73d9096fd",
}


class UnsignedXesEvidenceTests(unittest.TestCase):
    def test_pinned_runtime_and_modules(self):
        self.assertEqual(REPORT["reference_version"], "2.7.22")
        self.assertEqual(REPORT["backend_version"], "0.6.2")
        self.assertEqual(
            REPORT["reference_commit"], "0715c31f3f1d85924107d6aa5329806db69df899"
        )
        modules = REPORT["reference_modules"]
        self.assertEqual(len(modules), len(PINNED_MODULES))
        self.assertEqual({m["module"] for m in modules}, set(PINNED_MODULES))
        for module in modules:
            self.assertEqual(
                module["installed_sha256"], PINNED_MODULES[module["module"]]
            )
            self.assertEqual(module["pinned_sha256"], PINNED_MODULES[module["module"]])

    def test_exact_boundary_manifest_and_outcomes(self):
        variants = (
            "ITERPARSE",
            "LINE_BY_LINE",
            "ITERPARSE_MEM_COMPRESSED",
            "ITERPARSE_20",
            "CHUNK_REGEX",
            "RUSTXES",
        )
        expected = [
            (value, variant)
            for value in (2**63 - 1, 2**63, 2**64 - 1, 2**64)
            for variant in variants
        ]
        self.assertEqual([(r["input"], r["variant"]) for r in REPORT["rows"]], expected)
        for row in REPORT["rows"]:
            self.assertNotIn("error", row)
            self.assertEqual(row["type"], "int")
            self.assertIs(type(row["value"]), int)
            self.assertIn('value="' + str(row["input"]) + '"', row["xml"])
            expected = (
                0
                if row["variant"] == "RUSTXES" and row["input"] >= 2**63
                else row["input"]
            )
            self.assertEqual(row["value"], expected)


if __name__ == "__main__":
    unittest.main()
