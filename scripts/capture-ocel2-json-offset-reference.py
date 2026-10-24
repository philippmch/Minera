"""Record how the public OCEL 2.0 JSON reader stores timestamp offsets.

Only ``pm4py.read_ocel2_json`` and ``pm4py.discover_ocdfg`` are called. The
result documents an importer behavior that sits outside the OCDFG algorithm
gate; Minera's own reader keeps the real instant.
"""
import json
import sys
import tempfile
import warnings
from pathlib import Path

CASES = {
    "same-offset": ["2034-02-03T16:30:00+02:00", "2034-02-06T08:30:00+02:00",
                    "2034-02-06T09:00:00.123456789+02:00"],
    "mixed-offsets": ["2034-02-03T16:30:00+02:00", "2034-02-06T08:30:00-05:00",
                      "2034-02-06T09:00:00Z"],
    "naive": ["2034-02-03T16:30:00", "2034-02-06T08:30:00", "2034-02-06T09:00:00"],
}


def main():
    warnings.filterwarnings("ignore")
    import pm4py
    rows = []
    with tempfile.TemporaryDirectory() as scratch:
        for name, times in CASES.items():
            document = {
                "objectTypes": [{"name": "y-type", "attributes": []}],
                "eventTypes": [{"name": activity, "attributes": []} for activity in "ABC"],
                "objects": [{"id": "y-o1", "type": "y-type"}],
                "events": [{"id": f"y-e{index}", "type": "ABC"[index], "time": time,
                            "relationships": [{"objectId": "y-o1", "qualifier": "q"}]}
                           for index, time in enumerate(times)],
            }
            path = Path(scratch) / f"{name}.json"
            path.write_text(json.dumps(document))
            ocel = pm4py.read_ocel2_json(str(path))
            column = ocel.events["ocel:timestamp"]
            row = {"case": name, "input": times, "dtype": str(column.dtype),
                   "stored": [value.isoformat() for value in column]}
            for business_hours in (False, True):
                graph = pm4py.discover_ocdfg(ocel, business_hours=business_hours)
                row["business_hours" if business_hours else "elapsed"] = {
                    f"{source}->{target}": [float(value).hex() for value in values]
                    for (source, target), values in
                    graph["edges_performance"]["event_couples"]["y-type"].items()}
            rows.append(row)
    json.dump({"pm4py": pm4py.__version__, "rows": rows}, sys.stdout, indent=1)


if __name__ == "__main__":
    main()
