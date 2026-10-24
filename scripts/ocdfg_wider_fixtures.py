"""Deterministic disjoint OCEL fixtures for the wider OCDFG discovery gate.

The fixtures are generated from fixed seeds only, so the reference capture and
the native check rebuild identical inputs. Every identifier carries a ``w``
prefix and every timestamp lies in 2029-2032, keeping the bank disjoint from
the January 2026 incidence matrices and the February 2027 timing bank.
"""
import datetime
import random

PROFILES = {
    # name: (business_hours, slots); slots None means the public default.
    "elapsed": (False, None),
    "default-week": (True, None),
    "split-fractional": (True, [[0, 109800], [181800.5, 194400], [432000, 604800]]),
    "overlap-unsorted": (True, [[400000, 500000], [100000, 450000], [36000, 43200.25]]),
    "empty": (True, []),
}

ZONES = {
    "utc": datetime.timezone.utc,
    "plus0530": datetime.timezone(datetime.timedelta(hours=5, minutes=30)),
    "minus0800": datetime.timezone(datetime.timedelta(hours=-8)),
    "naive": None,
}


def _iso(moment, zone):
    if zone is None:
        text = moment.strftime("%Y-%m-%dT%H:%M:%S")
        if moment.microsecond:
            text += f".{moment.microsecond:06d}"
        return text
    local = moment.replace(tzinfo=datetime.timezone.utc).astimezone(zone)
    text = local.strftime("%Y-%m-%dT%H:%M:%S")
    if local.microsecond:
        text += f".{local.microsecond:06d}"
    offset = local.strftime("%z")
    return text + ("Z" if offset == "+0000" else offset[:3] + ":" + offset[3:])


def _random_fixture(name, seed, zone_name, *, events_range, types_range,
                    objects_range, span_days, sorted_rows, base):
    rng = random.Random(seed)
    zone = ZONES[zone_name]
    activity_count = rng.randint(3, 8)
    activities = [f"w{seed}-act{index}" for index in range(activity_count)]
    types = [f"w{seed}-type{index}" for index in range(rng.randint(*types_range))]
    objects = []
    for index in range(rng.randint(*objects_range)):
        objects.append((f"w{seed}-o{index}", rng.choice(types)))
    moments = []
    current = base
    for _ in range(rng.randint(*events_range)):
        roll = rng.random()
        if moments and roll < 0.18:
            pass  # tied timestamp with the previous event
        else:
            step = rng.expovariate(1.0 / (span_days * 86400.0 / 60.0))
            current = current + datetime.timedelta(seconds=round(step, 0))
            if rng.random() < 0.3:
                current = current.replace(microsecond=rng.randrange(1, 1000000))
        moments.append(current)
    events = []
    for index, moment in enumerate(moments):
        events.append((f"w{seed}-e{index}", rng.choice(activities), _iso(moment, zone)))
    relations = []
    for event_id, _, _ in events:
        for object_id, _ in rng.sample(objects, k=min(len(objects), rng.choice([0, 1, 1, 2, 2, 3, 4]))):
            relations.append((event_id, object_id, rng.choice(["w-in", "w-out", "w-ref"])))
            if rng.random() < 0.08:
                relations.append((event_id, object_id, "w-dup"))
    if not sorted_rows:
        order = list(range(len(events)))
        rng.shuffle(order)
        events = [events[index] for index in order]
    rank = {event[0]: index for index, event in enumerate(events)}
    relations.sort(key=lambda relation: rank[relation[0]])
    return {"name": name, "zone": zone_name, "events": events,
            "objects": objects, "relations": relations}


def _handcrafted():
    zone = ZONES["utc"]
    base = datetime.datetime(2031, 9, 5, 16, 45)  # Friday afternoon
    stamp = lambda hours: _iso(base + datetime.timedelta(hours=hours), zone)
    events = [
        ("wh-e0", "w Start ✓", stamp(0)),
        ("wh-e1", "w->arrow", stamp(64.25)),       # Monday 09:00
        ("wh-e2", "w->arrow", stamp(64.25)),       # tie, same activity
        ("wh-e3", "w Lone", stamp(70)),            # no objects
        ("wh-e4", "w End", stamp(24 * 7 + 1)),
        ("wh-e5", "w Start ✓", stamp(24 * 9 - 0.5)),
    ]
    objects = [("wh-o0", "w cart"), ("wh-o1", "w cart"), ("wh-o2", "w Ünit"),
               ("wh-unused", "w cart"), ("wh-solo", "w solo-type")]
    relations = [
        ("wh-e0", "wh-o0", "a"), ("wh-e0", "wh-o2", "a"),
        ("wh-e1", "wh-o0", "a"), ("wh-e1", "wh-o0", "b"),
        ("wh-e2", "wh-o0", "a"), ("wh-e2", "wh-o1", "a"),
        ("wh-e4", "wh-o1", "a"), ("wh-e4", "wh-o2", "a"),
        ("wh-e5", "wh-o2", "a"), ("wh-e5", "wh-solo", "a"),
    ]
    return {"name": "handcrafted-boundaries", "zone": "utc", "events": events,
            "objects": objects, "relations": relations}


def fixtures():
    result = []
    seed = 7100
    base = datetime.datetime(2029, 5, 7, 6, 15)
    for index in range(24):
        seed += 1
        result.append(_random_fixture(
            f"random-utc-{index:02d}", seed, "utc", events_range=(20, 150),
            types_range=(2, 4), objects_range=(5, 40), span_days=120,
            sorted_rows=index % 3 == 0, base=base))
    for zone_name in ("plus0530", "minus0800", "naive"):
        for index in range(8):
            seed += 1
            result.append(_random_fixture(
                f"random-{zone_name}-{index:02d}", seed, zone_name,
                events_range=(15, 90), types_range=(1, 3), objects_range=(3, 20),
                span_days=40, sorted_rows=index % 2 == 0,
                base=datetime.datetime(2029, 11, 2, 3, 30)))
    for index in range(4):
        seed += 1
        result.append(_random_fixture(
            f"long-lifetime-{index:02d}", seed, "utc", events_range=(200, 320),
            types_range=(1, 2), objects_range=(2, 4), span_days=900,
            sorted_rows=index % 2 == 1,
            base=datetime.datetime(2030, 1, 3, 12, 0)))
    result.append(_handcrafted())
    return result


def ocel2_json(fixture):
    """Standard OCEL 2.0 JSON with relations nested in event row order."""
    by_event = {}
    for event_id, object_id, qualifier in fixture["relations"]:
        by_event.setdefault(event_id, []).append(
            {"objectId": object_id, "qualifier": qualifier})
    return {
        "objectTypes": [{"name": name, "attributes": []}
                        for name in sorted({kind for _, kind in fixture["objects"]})],
        "eventTypes": [{"name": name, "attributes": []}
                       for name in sorted({activity for _, activity, _ in fixture["events"]})],
        "objects": [{"id": object_id, "type": kind} for object_id, kind in fixture["objects"]],
        "events": [{"id": event_id, "type": activity, "time": time,
                    "relationships": by_event.get(event_id, [])}
                   for event_id, activity, time in fixture["events"]],
    }
