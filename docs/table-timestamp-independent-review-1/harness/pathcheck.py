import sys, types, os, tempfile, pathlib
src = open(sys.argv[1], 'rb').read()
ns = {'__name__': 'probe', '__file__': sys.argv[1]}; exec(compile(src, sys.argv[1], 'exec'), ns)
root = pathlib.Path(tempfile.mkdtemp(dir=sys.argv[2])); ns['ROOT'] = root
(root / 'real').mkdir(); (root / 'real/f.txt').write_bytes(b'x')
os.symlink(root / 'real', root / 'linkdir'); os.symlink(root / 'real/f.txt', root / 'link.txt'); os.symlink(root / 'nowhere', root / 'dangling')
outside = pathlib.Path(tempfile.mkdtemp(dir=sys.argv[2])); (outside / 'secret').write_bytes(b's')
os.symlink(outside, root / 'escape')
def t(label, f):
    try: print(label, '->', f())
    except Exception as e: print(label, '-> REJECT', type(e).__name__, e)
p = ns['path']
t('regular', lambda: p('real/f.txt').read_bytes())
t('symlinked file', lambda: p('link.txt').read_bytes())
t('symlinked dir component', lambda: p('linkdir/f.txt').read_bytes())
t('escape dir symlink', lambda: p('escape/secret').read_bytes())
t('dangling link returned by path()', lambda: str(p('dangling')))
t('dangling link read', lambda: p('dangling').read_bytes())
t('dangling link mkdir', lambda: p('dangling').mkdir(parents=False, exist_ok=False))
t('dangling link open xb', lambda: p('dangling').open('xb'))
t('dotdot', lambda: p('real/../real/f.txt'))
t('absolute', lambda: p('/etc/passwd'))
print('nowhere created?', (root / 'nowhere').exists())
