"""Pinned public PNML serialization observations; no implementation inspection."""
import argparse
import inspect
import json
from pathlib import Path
import xml.etree.ElementTree as ET

import pm4py
from pm4py.objects.petri_net.obj import PetriNet, Marking
from pm4py.objects.petri_net.utils.petri_utils import add_arc_from_to
from pm4py.objects.petri_net.exporter import exporter
from pm4py.objects.petri_net.importer import importer

assert pm4py.__version__ == '2.7.22'
root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--output', type=Path, default=root / 'build/pnml-marking-export-reference.json')
parser.add_argument('--case', action='append', help='Capture only the named case; repeat to select several')
args = parser.parse_args()
out = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
           public_parameters={name: [dict(name=p.name, kind=str(p.kind), required=p.default is inspect.Parameter.empty)
                                     for p in inspect.signature(fn).parameters.values()] for name, fn in
                       [('exporter.serialize', exporter.serialize), ('importer.deserialize', importer.deserialize)]},
           observations=[])
for name, places, initial, final, arcs in [
    ('sequence_positive', ['entry', 'exit'], {'entry': 1}, {'exit': 1}, [('entry', 'move', 1), ('move', 'exit', 1)]),
    ('sequence_zero_keys', ['entry', 'exit'], {'entry': 1, 'exit': 0}, {'entry': 0, 'exit': 1}, [('entry', 'move', 1), ('move', 'exit', 1)]),
    ('sequence_zero_boundaries', ['entry', 'exit'], {'entry': 0}, {'exit': 0}, [('entry', 'move', 1), ('move', 'exit', 1)]),
    ('sequence_empty_markings', ['entry', 'exit'], {}, {}, [('entry', 'move', 1), ('move', 'exit', 1)]),
    ('weighted_multiplace', ['entry', 'extra', 'exit'], {'entry': 3, 'extra': 2, 'exit': 0}, {'entry': 0, 'extra': 1, 'exit': 2}, [('entry', 'move', 2), ('extra', 'move', 1), ('move', 'exit', 2)]),
    ('cyclic_empty_markings', ['left', 'right'], {}, {}, [('left', 'move', 1), ('move', 'left', 1)]),
    ('escaped_ids', ['entry&"', 'exit<'], {'entry&"': 1}, {'exit<': 1}, [('entry&"', 'move', 1), ('move', 'exit<', 1)]),
    ('whitespace_ids', ['a\nb', 'a b', 'a\tb', 'a\rb'], {'a\nb': 1, 'a\tb': 0}, {'a b': 1, 'a\rb': 0}, [('a\nb', 'move', 1), ('move', 'a b', 1)]),
    ('empty_net', [], {}, {}, []),
]:
    if args.case and name not in args.case: continue
    net = PetriNet(name)
    nodes = {p: PetriNet.Place(p) for p in places}
    net.places.update(nodes.values())
    if places:
        nodes['move'] = PetriNet.Transition('move', 'A')
        net.transitions.add(nodes['move'])
    for a, b, weight in arcs: add_arc_from_to(nodes[a], nodes[b], net, weight=weight)
    im = Marking({nodes[p]: n for p, n in initial.items()})
    fm = Marking({nodes[p]: n for p, n in final.items()})
    row = dict(name=name, net=dict(places=places, initial=initial, final=final, arcs=arcs))
    try:
        xml = exporter.serialize(net, im, final_marking=fm)
        row['pnml'] = xml.decode('utf-8') if isinstance(xml, bytes) else xml
        tree = ET.fromstring(row['pnml'])
        for node in tree.iter(): node.tag = node.tag.rsplit('}', 1)[-1]
        row['exported_initial'] = {p.attrib['id']: int(p.findtext('initialMarking/text'))
                                  for p in tree.iter('place') if p.find('initialMarking/text') is not None}
        row['exported_final'] = {p.attrib['idref']: int(p.findtext('text'))
                                for p in tree.findall('.//finalmarkings/marking/place')}
        row['exported_place_ids'] = sorted(p.attrib['id'] for p in tree.iter('place') if 'id' in p.attrib)
        row['exported_transition_ids'] = sorted(t.attrib['id'] for t in tree.iter('transition'))
        try:
            imported, imported_initial, imported_final = importer.deserialize(xml)
            row['roundtrip_initial'] = {p.name: n for p, n in imported_initial.items()}
            row['roundtrip_final'] = {p.name: n for p, n in imported_final.items()}
        except Exception as error:
            row['import_error'] = dict(type=type(error).__name__, message=str(error))
    except Exception as error:
        row['export_error'] = dict(type=type(error).__name__, message=str(error))
    out['observations'].append(row)
args.output.write_text(json.dumps(out, indent=2) + '\n', encoding='utf-8')
for row in out['observations']:
    print(json.dumps({k: v for k, v in row.items() if k not in ('pnml', 'net')}, sort_keys=True))
