"""Compare raw native sample fitting with pinned public RandomVariable outputs."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

path=Path(__file__).with_name('check-stochastic-fit-reference.py')
spec=importlib.util.spec_from_file_location('fit_checker',path)
checker=importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--probe',type=Path,required=True)
    parser.add_argument('--fixtures',type=Path,
        default=Path(__file__).resolve().parents[1]/'docs/random-variable-fit-reference-fixtures.json')
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    checks=[]
    for row in json.loads(args.fixtures.read_text())['observations']:
        sample=row['spec']
        force=sample['force_distribution']
        result=subprocess.run([str(args.probe.resolve()),'--samples',force or 'automatic',
            ','.join(map(str,sample['values']))],capture_output=True,text=True,timeout=20)
        name=sample['name']+'_'+str(force)
        if 'error' in row:
            assert result.returncode!=0 and 'negative' in result.stderr, (name,result)
        else:
            assert result.returncode==0, (name,result.stderr)
            actual=checker.records({'map':json.loads(result.stdout)},['sample'])['sample']
            checker.compare(actual,checker.expected_record(row['result']),name)
        checks.append(dict(name=name,passed=True))
    args.output.write_text(json.dumps(dict(passed=len(checks),checks=checks),indent=2)+'\n')
    print(len(checks),'raw distribution comparisons passed')

if __name__=='__main__':
    main()
