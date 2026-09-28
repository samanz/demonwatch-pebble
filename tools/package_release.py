"""Package a release: the watch app, the complete corresponding source (as the
GPL requires), store screenshots and SHA-256 sums, into dist/<version>/.

The source zip is `git archive` of HEAD, so it is exactly the committed code;
the script refuses to run with uncommitted changes. Build first
(`sh tools/check_all.sh --emulator` also refreshes the screenshots).

Usage: python3 tools/package_release.py
"""
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCREENSHOTS = {   # emulator captures from tools/check_maps.py -> store names
    'work/title.png': '0-title.png',
    'work/difficulty.png': '1-difficulty.png',
    'work/map1-look0.png': '2-hangar-gate.png',
    'work/map2-start.png': '3-toxin-refinery.png',
    'work/map3-start.png': '4-command-center.png',
    'work/map1-automap.png': '5-automap.png',
    'work/map1-clear.png': '6-level-clear.png',
}


def git(*args):
    return subprocess.run(['git', *args], cwd=ROOT, check=True, capture_output=True, text=True).stdout


if git('status', '--porcelain', '--untracked-files=no').strip():
    sys.exit('Commit your changes first: the source zip must match the app.')
version = json.loads((ROOT / 'package.json').read_text())['version']
name = f'demonwatch-{version}'
out = ROOT / 'dist' / version
out.mkdir(parents=True, exist_ok=True)

shutil.copyfile(ROOT / 'build/pdoom.pbw', out / f'{name}.pbw')
subprocess.run(['git', 'archive', '--format=zip', f'--prefix={name}/', '-o', str(out / f'{name}-source.zip'), 'HEAD'],
               cwd=ROOT, check=True)
shots = out / 'screenshots'
shots.mkdir(exist_ok=True)
for src, dst in SCREENSHOTS.items():
    if (ROOT / src).exists():
        shutil.copyfile(ROOT / src, shots / dst)
    else:
        print(f'missing screenshot {src} (run tools/check_maps.py)')
sums = ''.join(f'{hashlib.sha256((out / n).read_bytes()).hexdigest()}  {n}\n'
               for n in (f'{name}.pbw', f'{name}-source.zip'))
(out / 'SHA256SUMS.txt').write_text(sums)
print(f'Packaged {name} ({git("rev-parse", "--short", "HEAD").strip()}) in {out.relative_to(ROOT)}')
