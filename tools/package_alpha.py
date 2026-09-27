"""Package the current PBW and its corresponding source for local delivery."""
import hashlib
import shutil
import sys
import zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for src,dst in [('build/pdoom.pbw','pdoom-alpha.pbw'),('README.md','pdoom-README.md'),('work/pause-restart.png','pdoom-gameplay.png'),('work/enemy.png','pdoom-combat.png'),('work/exit.png','pdoom-level-clear.png')]:
    shutil.copyfile(root/src,out/dst)
files=[root/n for n in ['README.md','LICENSE','package.json','wscript','resources/arena.pbl','resources/freedoom-art.pbl','resources/menu_icon.png']]
for folder in ['src/doom','src/pebble','licenses','tests']:
    files.extend(f for f in (root/folder).rglob('*') if f.is_file() and '__pycache__' not in f.parts)
files.extend(root/'tools'/n for n in ['build_arena.py','import_freedoom.py','run_pebble.py','verify_pbw.py','check_playthrough.py','check_menus.py','package_alpha.py','build_icon.py'])
with zipfile.ZipFile(out/'pdoom-alpha-source.zip','w',zipfile.ZIP_DEFLATED) as z:
    for f in sorted(set(files)): z.write(f,'pdoom/'+f.relative_to(root).as_posix())
with zipfile.ZipFile(out/'pdoom-alpha-source.zip') as z: assert z.testzip() is None
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256((out/n).read_bytes()).hexdigest()+'  '+n+'\n' for n in ['pdoom-alpha.pbw','pdoom-alpha-source.zip']),encoding='utf-8')
print('Packaged watch app, source, controls, screenshots, and SHA256 hashes.')
