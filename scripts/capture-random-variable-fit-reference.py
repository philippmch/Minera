"""Capture pinned RandomVariable public fitting outputs without source inspection."""
import argparse
import json
from pathlib import Path
import warnings

warnings.filterwarnings('ignore')
import numpy
import scipy
import pm4py
from pm4py.objects.random_variables.random_variable import RandomVariable

assert pm4py.__version__ == '2.7.22'

SAMPLES = {
    'empty': [], 'zero': [0], 'zeros': [0, 0, 0],
    'one_positive': [2], 'positive_constant': [2, 2, 2],
    'two_positive': [2, 4], 'three_positive': [1, 2, 3],
    'one_negative': [-4], 'negative_constant': [-2, -2, -2],
    'negative_mean': [-5, 1, 2], 'zero_mean': [-1, 0, 1],
    'positive_mixed': [-1, 2, 3], 'zero_positive': [0, 1, 2],
    'tiny_positive': [1e-12, 2e-12], 'near_zero': [-1e-6, 0, 1e-6],
    'uniform_grid': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10],
    'normal_cluster': [2, 3, 4, 4, 5, 5, 5, 6, 6, 7, 8],
    'long_tail': [0.1, 0.2, 0.3, 0.5, 0.8, 1.3, 2.1, 3.4, 5.5, 8.9],
    'normal_dominant': [1, *([5] * 20), 9],
    'negative_varying': [-4, -2, -1],
    'near_zero_inside': [0, 0.00009999],
    'near_zero_boundary': [0, 0.0001],
    'near_zero_outside': [0, 0.00010001],
}
FORCED = [None, 'EXPONENTIAL', 'NORMAL', 'UNIFORM', 'LOGNORMAL', 'GAMMA',
          'DETERMINISTIC', 'IMMEDIATE', 'CONSTANT0', 'normal', 'invalid']


def capture(name, values, force):
    spec = dict(name=name, values=values, force_distribution=force)
    try:
        variable = RandomVariable()
        variable.calculate_parameters(values, force_distribution=force)
        return dict(spec=spec, result=dict(distribution=variable.get_distribution_type(),
            parameters=variable.get_distribution_parameters(), weight=variable.get_weight(),
            priority=variable.get_priority()))
    except Exception as error:
        return dict(spec=spec, error=dict(type=type(error).__name__, message=str(error)))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    rows = [capture(name, values, force) for name, values in SAMPLES.items() for force in FORCED]
    document = dict(reference='2.7.22', reference_commit='0715c31f3f1d85924107d6aa5329806db69df899',
        public_api='pm4py.objects.random_variables.random_variable.RandomVariable.calculate_parameters',
        dependencies=dict(numpy=numpy.__version__, scipy=scipy.__version__), observations=rows)
    args.output.write_text(json.dumps(document, indent=2) + '\n', encoding='utf-8')
    for row in rows:
        print(json.dumps(dict(name=row['spec']['name'], force=row['spec']['force_distribution'],
            **{key: value for key, value in row.items() if key != 'spec'})))


if __name__ == '__main__':
    main()
