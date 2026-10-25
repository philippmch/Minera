# Common and DFG filtering helpers: public-call bank (2026-10-25)

This checkpoint closes the four partial filtering support modules

* `algo.filtering.common.filtering_constants`
* `algo.filtering.common.timestamp.timestamp_common`
* `algo.filtering.common.attributes.attributes_common`
* `algo.filtering.dfg.dfg_filtering`

with one seeded bank of public calls. It ran in the cloud container on the
pinned runtime (Python 3.11.17, pm4py 2.7.22, pandas 3.0.5, numpy 2.4.6,
scipy 1.17.1, networkx 3.6.1). Only public functions, signatures and observed
results were used. No reference source was read. The inputs that numpy and
scipy received were observed only to fix the numerics described below.

## Native API

* `include/minera/filtering_common.hpp` and `src/backend/filtering_common.cpp`
  hold the constants (`kFilteringCaseConceptName`,
  `kFilteringDecreasingFactor`, `kFilteringDefaultNoiseThreshDf`),
  `get_dt_from_string`, `get_sorted_attributes_list`,
  `get_attributes_threshold`, and the numeric and date KDE functions with
  their JSON forms. Public failures raise `FilteringCommonError` with the
  Python class name and message.
* `include/minera/dfg_filtering.hpp` and `src/backend/dfg_filtering.cpp` hold
  the DFG filters over insertion-ordered dictionaries (`DfgFilteringData`)
  next to the older `dfg_filter.hpp` API, which is unchanged.

## Tooling

* `scripts/filtering_common_domain.py` generates the cases.
  * DFG cases have 0-7 activities with counts, random edges (zero, negative,
    10**6 and 2**40 counts included), start and end activities, and
    inconsistent variants whose edges or boundaries name unknown activities.
  * Percentages, thresholds and noise values include 0, 1, negatives, NaN and
    the infinities. Adjacency arguments come as sets, lists or tuples.
  * KDE inputs have 0-60 values: constants, tiny and huge ranges, negative
    ranges, NaN and infinities. Dates cover the Europe/Berlin DST changes,
    microseconds and naive, UTC and offset-aware values. `graph_points` and
    `points_to_sample` take 0, negatives, 1 and the defaults, by name or by
    `Parameters` member.
  * `get_dt_from_string` strings mutate one field (out-of-range months, days,
    hours, leap days) and add leading or trailing text. Its other arguments
    are datetimes (naive and aware), ints, floats, booleans, None and lists.
* `scripts/capture-filtering-common-bank.py` runs every call in fresh
  interpreters for `PYTHONHASHSEED` 0-3 with `TZ=Europe/Berlin` and writes
  `docs/filtering-common-bank.json.gz`. It records values, errors, and
  arguments the call mutated. A call whose outcome differs between hash seeds
  keeps all observed outcomes.
* `minera_filtering_common_bank_parity_tests`
  (`src/backend/tests/test_filtering_common_bank_parity.cpp`) replays each
  call through the native API, with one function per module. The ctest
  registration pins `TZ=Europe/Berlin`.

## Final bank

Bank seed 20261026, 4,000 cases, hash seeds 0-3:

| Calls | Values | Errors | Mutated arguments | Varying across seeds | Failures |
| --- | --- | --- | --- | --- | --- |
| 33,747 | 26,978 | 6,769 | 746 | 5 | 0 |

SHA-256 of the bank: `6fa01e508bae1a89b778f681eac08e89166e90e265a822f964f17220e92e2edc`.
Pilots with bank seed 7 drove the fixes and are not part of the evidence.

## Comparison rules

* Everything except KDE densities compares exactly. That covers values as
  typed JSON with floats bit-exact, error class and message, mutated
  arguments, dictionary order, the linspace grids, the datetime64 grids and
  the JSON seconds.
* Sets and dictionaries whose order follows string hashing compare unordered:
  the `build_adjacency_structures` maps and the `bfs_reachable` result.
* When several activities are missing, the reference raises the KeyError of
  whichever one its set iteration meets first, and that depends on the hash
  seed. The test accepts a KeyError naming any of the missing names; 36 calls
  needed this.
* KDE densities use a tolerance. scipy's `numpy.cov` goes through BLAS
  `gemm`, whose summation order is not portable. A covariance that differs in
  its last bits moves each density by about `eps * max|x| / span` relative,
  so the density tolerance is `1e-9 + 1e-13 * max|x| / span`, plus an
  absolute `1e-12 * max(y)` for the far tails. A one-point grid uses the span
  of the input values. The geomspace points of the numeric grid use relative
  tolerance 1e-12, because numpy's SIMD `log10`/`pow` differ from libm.
  The worst density difference in the bank is 2.6e-6 relative, which is 0.05
  of its tolerance (4,772 KDE calls).

## Behaviour established

### timestamp_common

* `get_dt_from_string` parses with strptime `%Y-%m-%d %H:%M:%S` and returns
  the UTC-aware datetime with the parsed fields. Unicode digits and whitespace
  are accepted, as Python's `re` matches them.
* The errors are tried in this order:
  1. the "does not match format" error
  2. "unconverted data remains"
  3. the year range
  4. "day is out of range for month"
  5. "second must be in 0..59"
* A datetime argument keeps its wall clock and gets UTC as its zone. Other
  types raise the AttributeError of `replace`, and `bytes` raises
  `bytes.replace()`'s TypeError.

### attributes_common

* `get_sorted_attributes_list` sorts by count, descending and stable.
* `get_attributes_threshold` starts at index
  `max(0, min(min_count - 1, len - 1))`. For each later index it raises the
  threshold to any count greater than `threshold * factor`, and it stops after
  index `max_count`.
* Numeric KDE:
  * Empty input gives `[]`. One distinct value gives a spike grid.
  * Otherwise the grid is the sorted union of linspace and, for ranges of one
    sign, geomspace (negated for negative ranges), each with
    `max(graph_points // 2, 2)` points.
  * NaN or infinite input raises scipy's ValueError.
* Date KDE:
  * Values are naive wall clocks turned into local `timestamp()` seconds.
  * Samples are taken at positions `floor(i * n / k)` when `k >= n`.
    Otherwise they are taken at `n // (2k) + floor(i * n / k)` plus the two
    ends. When `k <= 0`, only the ends are kept.
  * The grid is the truncated `datetime64[ns]` linspace. Its JSON seconds are
    rounded to 6 digits through `Timestamp.timestamp()`.
  * Empty input raises IndexError.

### dfg_filtering

* `build_adjacency_structures` resets the start and end entries before it
  adds the boundary arcs.
* `bfs_reachable` raises KeyError for a reached node without an entry.
* `remove_unreachable_nodes` removes the dropped activities from the
  activity, start and end dictionaries in place.
* Activity and path percentage filters:
  * They keep `ceil((n - 1) * p) + 1` entries, with Python slice semantics.
    NaN raises ValueError and infinity raises OverflowError.
  * The other entries are tried for removal in reverse order. An entry is
    dropped when every kept activity stays connected.
* `filter_dfg_keep_connected` gives a division-by-zero error when an arc's
  count and inverse count sum to -1. A NaN threshold keeps every arc.
* In `clean_dfg_based_on_noise_thresh` the per-activity maxima start at zero,
  so negative-count edges are dropped for any threshold. A None or missing
  most-common-paths argument means an empty set.
* `filter_dfg_to_activity`, `filter_dfg_from_activity` and
  `filter_dfg_contain_activity` check the target first, then the edges and
  boundaries, before restricting to the reachable activities.
* `generate_nx_graph_from_dfg` adds two fresh uuid4 nodes. The test checks
  their shape and compares the rest of the graph exactly.

## Contract and domain limits

All four modules are complete on this bank. Activity names equal to the
reference's reserved node names `_S_START_` and `_S_END_` are outside the
domain: the reference mixes them up with its own boundary nodes, and the
native API does not reproduce that.
