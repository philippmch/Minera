"""Capture workflow-net to process-tree observations from PM4Py 2.7.22.

Only public entry points are called; no native code runs. Inputs are built
here from fixed seeds and literals:

* fixed process trees (minimal concurrency/choice/loop shapes and three seeded
  random trees), converted to workflow nets by the public tree converter;
* the public BPMN round trip of one seeded tree (tree -> BPMN -> Petri net);
* a literal heuristics-style net with two loops sharing an exit place;
* heuristics nets from seeded playouts that the public
  `convert_to_process_tree` converts, and small alpha nets from seeded
  playouts that it rejects.

For every accepted net the capture records its complete language up to six
visible events, checked to equal the language of the reference's converted
tree. Rejected nets are recorded as PNML only. Nets are written as compact
PNML with canonical element ids, so the output does not depend on PM4Py's
random transition names.

    python -I -B scripts/capture-workflow-net-tree-reference.py \\
        --output src/backend/tests/workflow_net_tree_reference_cases.inc
"""
import argparse
import datetime
import os
import random
import sys
import warnings
from pathlib import Path
from xml.sax.saxutils import escape

# Alpha place names and set iteration follow string hashing; fix it so the
# seeded inputs are reproducible.
if os.environ.get("PYTHONHASHSEED") != "0":
    os.environ["PYTHONHASHSEED"] = "0"
    os.execv(sys.executable, [sys.executable] + sys.orig_argv[1:])

warnings.filterwarnings("ignore")

import numpy  # noqa: E402
import pm4py  # noqa: E402
from pm4py.algo.simulation.playout.process_tree import algorithm as tree_playout  # noqa: E402
from pm4py.algo.simulation.tree_generator import algorithm as tree_generator  # noqa: E402
from pm4py.objects.bpmn.exporter import exporter as bpmn_exporter  # noqa: E402
from pm4py.objects.bpmn.importer import importer as bpmn_importer  # noqa: E402
from pm4py.objects.conversion.bpmn import converter as bpmn_converter  # noqa: E402
from pm4py.objects.petri_net import semantics  # noqa: E402
from pm4py.objects.petri_net.obj import Marking, PetriNet  # noqa: E402
from pm4py.objects.petri_net.utils import petri_utils  # noqa: E402
from pm4py.objects.process_tree.utils.generic import parse  # noqa: E402

MAX_LENGTH = 6
TREES = [
    "->( 'x', *( 'a', 'b' ) )",
    "->( *( 'a', 'b' ), 'x' )",
    "+( X( 'a', 'b' ), 'c' )",
    "+( X( 'a', tau ), 'c' )",
    "+( 'a', *( 'b', 'c' ) )",
    "X( ->( X( 'a', 'd' ), X( 'g', tau ) ), 'b' )",
    "->( +( 'c', *( 'c', 'b' ) ), +( 'b', 'a' ) )",
    "+( tau, 'a', 'b' )",
    "+( X( 'a', 'c' ), X( 'b', X( 'e', 'd' ) ) )",
    "->( +( 'c', ->( 'e', X( tau, *( 'b', ->( 'f', tau ) ) ) ) ), "
    "->( 'd', +( 'a', 'g' ) ) )",
]
BPMN_TREE = "+( X( 'b', X( 'a', tau ) ), ->( tau, *( tau, *( tau, 'a' ) ) ) )"
# Places, then (label or None, inputs, outputs) per transition.
SHARED_EXIT_NET = (
    ["source", "pre_a", "pre_b", "pre_c", "pre_e", "pre_f", "exit", "sink"],
    [(None, ["source"], ["pre_a"]), ("a", ["pre_a"], ["pre_b"]),
     ("b", ["pre_b"], ["pre_c"]), ("c", ["pre_c"], ["pre_e"]),
     ("e", ["pre_e"], ["pre_f"]), ("f", ["pre_f"], ["exit"]),
     ("d", ["exit"], ["pre_f"]), (None, ["exit"], ["pre_a"]),
     (None, ["exit"], ["sink"])],
)
SEEDS = range(1000, 1250)
HEURISTICS_COUNT = 3
HEURISTICS_MAX_TRANSITIONS = 12
REJECT_COUNT = 4
REJECT_MAX_TRANSITIONS = 9


def seeded_log(seed):
    """A random tree and its playout, both drawn from `seed`."""
    random.seed(seed)
    numpy.random.seed(seed)  # the generator's triangular draw uses numpy
    tree = tree_generator.apply(
        variant=tree_generator.Variants.PTANDLOGGENERATOR,
        parameters={
            "mode": random.randint(3, 10), "min": 2, "max": 12,
            "silent": random.choice([0, 0.2, 0.4]),
            "duplicate": random.choice([0, 0.2]),
            "loop": random.choice([0.1, 0.3]),
            "parallel": random.choice([0.2, 0.4]), "or": 0.0,
        })
    log = tree_playout.apply(tree, parameters={"num_traces": 30})
    start = datetime.datetime(2020, 1, 1)
    for trace in log:
        for index, event in enumerate(trace):
            event["time:timestamp"] = start + datetime.timedelta(minutes=index)
    return log


def language(net, initial, final, max_length):
    """Every visible trace of length <= max_length reaching the final marking."""
    final_key = frozenset((place.name, count) for place, count in final.items())
    traces = set()
    seen = set()
    stack = [(initial, ())]
    while stack:
        marking, trace = stack.pop()
        key = (frozenset((place.name, count) for place, count in marking.items()), trace)
        if key in seen:
            continue
        seen.add(key)
        if key[0] == final_key:
            traces.add(trace)
        for transition in semantics.enabled_transitions(net, marking):
            extended = trace + ((transition.label,) if transition.label is not None else ())
            if len(extended) > max_length:
                continue
            following = semantics.execute(transition, net, marking)
            if sum(following.values()) > 30:
                raise RuntimeError("net is not bounded enough to enumerate")
            stack.append((following, extended))
    return traces


def canonical_order(net, initial, final):
    """Nodes in an order that ignores element names (colour refinement with
    individualisation), so random PM4Py ids never change the output."""
    nodes = list(net.places) + list(net.transitions)
    colour = {}
    for place in net.places:
        colour[place] = ("p", initial.get(place, 0), final.get(place, 0))
    for transition in net.transitions:
        colour[transition] = ("t", transition.label is not None, transition.label or "")

    def refine(colours):
        while True:
            signature = {
                node: (colours[node],
                       tuple(sorted(colours[arc.source] for arc in node.in_arcs)),
                       tuple(sorted(colours[arc.target] for arc in node.out_arcs)))
                for node in nodes}
            ranks = {value: index for index, value in enumerate(sorted(set(signature.values())))}
            refined = {node: (ranks[signature[node]],) for node in nodes}
            if len(set(refined.values())) == len(set(colours.values())):
                return refined
            colours = refined

    colours = refine({node: (0, colour[node]) for node in nodes})
    while len(set(colours.values())) < len(nodes):
        counts = {}
        for node in nodes:
            counts[colours[node]] = counts.get(colours[node], 0) + 1
        tied = min(value for value, count in counts.items() if count > 1)
        chosen = next(node for node in nodes if colours[node] == tied)
        colours = {node: (colours[node], node is chosen) for node in nodes}
        colours = refine({node: (0, colours[node]) for node in nodes})
    return sorted(nodes, key=lambda node: colours[node])


def compact_pnml(net, initial, final):
    """PNML with canonical element ids and arc order."""
    order = canonical_order(net, initial, final)
    places = [node for node in order if isinstance(node, PetriNet.Place)]
    transitions = [node for node in order if isinstance(node, PetriNet.Transition)]
    place_ids = {place: f"p{index}" for index, place in enumerate(places)}
    transition_ids = {transition: f"t{index}" for index, transition in enumerate(transitions)}
    ids = {**place_ids, **transition_ids}
    xml = '<pnml><net id="n"><page id="g">'
    for place in places:
        marking = (f"<initialMarking><text>{initial[place]}</text></initialMarking>"
                   if place in initial else "")
        xml += f'<place id="{place_ids[place]}">{marking}</place>'
    for transition in transitions:
        if transition.label is None:
            xml += (f'<transition id="{transition_ids[transition]}"><toolspecific '
                    'tool="ProM" activity="$invisible$"/></transition>')
        else:
            xml += (f'<transition id="{transition_ids[transition]}"><name><text>'
                    f'{escape(transition.label)}</text></name></transition>')
    arcs = sorted((ids[arc.source], ids[arc.target]) for arc in net.arcs)
    for index, (source, target) in enumerate(arcs):
        xml += f'<arc id="a{index}" source="{source}" target="{target}"/>'
    xml += "</page><finalmarkings><marking>"
    for place, count in sorted(final.items(), key=lambda item: place_ids[item[0]]):
        xml += f'<place idref="{place_ids[place]}"><text>{count}</text></place>'
    return xml + "</marking></finalmarkings></net></pnml>"


def converts(net, initial, final):
    try:
        pm4py.convert_to_process_tree(net, initial, final)
    except Exception:  # noqa: BLE001 - any public rejection counts
        return False
    return True


def is_workflow_shape(net):
    if any(not transition.in_arcs or not transition.out_arcs
           for transition in net.transitions):
        return False
    sources = [place for place in net.places if not place.in_arcs]
    sinks = [place for place in net.places if not place.out_arcs]
    return len(sources) == 1 and len(sinks) == 1


def bpmn_round_trip(tree_text, directory):
    path = directory / "workflow-net-tree-capture.bpmn"
    bpmn_exporter.apply(pm4py.convert_to_bpmn(parse(tree_text)), str(path))
    try:
        return bpmn_converter.apply(bpmn_importer.apply(str(path)))
    finally:
        path.unlink()


def literal_net(places, transitions):
    net = PetriNet("literal")
    by_name = {}
    for name in places:
        by_name[name] = PetriNet.Place(name)
        net.places.add(by_name[name])
    for index, (label, inputs, outputs) in enumerate(transitions):
        transition = PetriNet.Transition(f"t{index}", label)
        net.transitions.add(transition)
        for name in inputs:
            petri_utils.add_arc_from_to(by_name[name], transition, net)
        for name in outputs:
            petri_utils.add_arc_from_to(transition, by_name[name], net)
    return net, Marking({by_name[places[0]]: 1}), Marking({by_name[places[-1]]: 1})


def cpp_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()

    accepted = [(text, pm4py.convert_to_petri_net(parse(text))) for text in TREES]
    accepted.append((f"BPMN round trip of {BPMN_TREE}",
                     bpmn_round_trip(BPMN_TREE, arguments.output.parent)))
    accepted.append(("two loops sharing an exit place", literal_net(*SHARED_EXIT_NET)))
    logs = {seed: seeded_log(seed) for seed in SEEDS}
    heuristics = 0
    for seed, log in logs.items():
        net, initial, final = pm4py.discover_petri_net_heuristics(log)
        if (len(net.transitions) > HEURISTICS_MAX_TRANSITIONS or
                not is_workflow_shape(net) or not converts(net, initial, final)):
            continue
        accepted.append((f"heuristics net of seed {seed}", (net, initial, final)))
        heuristics += 1
        if heuristics == HEURISTICS_COUNT:
            break
    if heuristics != HEURISTICS_COUNT:
        raise RuntimeError("not enough convertible heuristics nets")

    lines = [
        "// Generated by scripts/capture-workflow-net-tree-reference.py from public",
        "// PM4Py 2.7.22: each net is listed with its complete language up to six",
        "// visible events, which the reference convert_to_process_tree result",
        "// shares.",
        "const std::vector<ReferenceWorkflowCase> kReferenceWorkflowCases{",
    ]
    for name, (net, initial, final) in accepted:
        tree = pm4py.convert_to_process_tree(net, initial, final)
        traces = language(net, initial, final, MAX_LENGTH)
        if traces != language(*pm4py.convert_to_petri_net(tree), MAX_LENGTH):
            raise RuntimeError(f"reference tree language differs for {name}")
        rows = ",\n      ".join(
            "{" + ", ".join(cpp_string(activity) for activity in trace) + "}"
            for trace in sorted(traces))
        lines.append(f"  {{{cpp_string(name)},\n   "
                     f"{cpp_string(compact_pnml(net, initial, final))},\n   {{{rows}}}}},")
    lines.append("};")

    rejected = []
    for seed, log in logs.items():
        net, initial, final = pm4py.discover_petri_net_alpha(log)
        if (len(net.transitions) > REJECT_MAX_TRANSITIONS or
                not is_workflow_shape(net) or converts(net, initial, final)):
            continue
        rejected.append((seed, compact_pnml(net, initial, final)))
        if len(rejected) == REJECT_COUNT:
            break
    if len(rejected) != REJECT_COUNT:
        raise RuntimeError("not enough rejected alpha nets")
    lines.append("// Workflow nets the reference cannot convert to a process tree.")
    lines.append("const std::vector<std::string> kReferenceRejectedWorkflowNets{")
    for seed, xml in rejected:
        lines.append(f"  {cpp_string(xml)},  // alpha net of seed {seed}")
    lines.append("};")
    arguments.output.write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
