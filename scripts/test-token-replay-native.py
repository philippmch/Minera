"""Unit contracts for validated token-replay result equivalence."""

import copy
import importlib.util
from pathlib import Path
import unittest


CHECKER_PATH = Path(__file__).resolve().with_name("check-token-replay-native.py")
specification = importlib.util.spec_from_file_location("replay_checker", CHECKER_PATH)
checker = importlib.util.module_from_spec(specification)
specification.loader.exec_module(checker)


class ReplayEquivalenceTests(unittest.TestCase):
    def setUp(self):
        self.case = {
            "net": {
                "places": ["l", "r", "lo", "ro"],
                "transitions": [["a", None], ["b", None]],
                "arcs": [
                    ["l", "a", 1],
                    ["a", "lo", 1],
                    ["r", "b", 1],
                    ["b", "ro", 1],
                ],
                "initial": {"l": 1, "r": 1},
                "final": {"lo": 1, "ro": 1},
            },
            "equivalent_element_renamings": [
                {"l": "r", "r": "l", "lo": "ro", "ro": "lo", "a": "b", "b": "a"}
            ],
        }
        self.context = checker.equivalence_context(self.case)
        self.trace = {
            "activated_transitions": [{"transition": "a"}, {"transition": "b"}],
            "transitions_with_problems": [],
            "reached_marking": {"marking": {"lo": 1}},
            "enabled_transitions_in_marking": {"set": []},
            "missing_tokens": 0,
            "trace_fitness": 1.0,
        }
        self.places = {
            place: dict(c=0, p=0, m=0, r=0)
            for place in ("l", "r", "lo", "ro")
        }

    def match_kind(self, expected, context=None, places=None):
        return checker.match_replay(
            [self.trace],
            self.places,
            [expected],
            self.places if places is None else places,
            self.context if context is None else context,
        )[0]

    def test_identity(self):
        self.assertEqual(self.match_kind(self.trace), "identity")

    def test_independent_hidden_swap(self):
        expected = copy.deepcopy(self.trace)
        expected["activated_transitions"].reverse()
        self.assertEqual(self.match_kind(expected), "independent_hidden_order")

    def test_shared_place_prevents_swap(self):
        expected = copy.deepcopy(self.trace)
        expected["activated_transitions"].reverse()
        self.case["net"]["arcs"].append(["lo", "b", 1])
        self.case["equivalent_element_renamings"] = []
        self.assertIsNone(
            self.match_kind(expected, checker.equivalence_context(self.case))
        )

    def test_visible_transition_prevents_swap(self):
        expected = copy.deepcopy(self.trace)
        expected["activated_transitions"].reverse()
        self.case["net"]["transitions"][0][1] = "A"
        self.case["equivalent_element_renamings"] = []
        self.assertIsNone(
            self.match_kind(expected, checker.equivalence_context(self.case))
        )

    def test_unequal_firing_counts(self):
        expected = copy.deepcopy(self.trace)
        expected["activated_transitions"].append({"transition": "a"})
        self.assertIsNone(self.match_kind(expected))

    def test_scalar_mismatch(self):
        expected = copy.deepcopy(self.trace)
        expected["missing_tokens"] = 1
        self.assertIsNone(self.match_kind(expected))

    def test_marking_value_mismatch(self):
        expected = copy.deepcopy(self.trace)
        expected["reached_marking"]["marking"] = {"ro": 2}
        self.assertIsNone(self.match_kind(expected))

    def test_problem_sequence_is_not_reordered(self):
        self.trace["transitions_with_problems"] = [
            {"transition": "a"}, {"transition": "b"}
        ]
        expected = copy.deepcopy(self.trace)
        expected["transitions_with_problems"].reverse()
        self.assertIsNone(self.match_kind(expected))

    def test_place_aggregate_mismatch(self):
        places = copy.deepcopy(self.places)
        places["l"]["c"] = 1
        self.assertIsNone(self.match_kind(self.trace, places=places))

    def test_combined_symmetry_and_hidden_swap(self):
        expected = copy.deepcopy(self.trace)
        expected["reached_marking"]["marking"] = {"ro": 1}
        self.assertEqual(self.match_kind(expected), "net_automorphism")

    def test_invalid_automorphism_marking(self):
        self.case["net"]["initial"]["l"] = 2
        with self.assertRaises(ValueError):
            checker.equivalence_context(self.case)

    def test_visible_barrier_cannot_be_crossed(self):
        labels = {"a": None, "b": None, "v": "V"}
        incident = {name: set() for name in labels}

        def sequence(names):
            return [{"transition": name} for name in names]

        self.assertFalse(
            checker.legal_hidden_reordering(
                sequence("avb"), sequence("bva"), labels, incident
            )
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
