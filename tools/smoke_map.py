"""Quick emulator look at a map: install, start a new game, take screenshots
while turning in place and walking a little, then report engine errors and
the zone/heap figures from the log.

Usage: python3 tools/smoke_map.py [build/pdoom.pbw] [name-prefix]
Screenshots go to work/<prefix>-*.png.
"""
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
pbw = sys.argv[1] if len(sys.argv) > 1 else 'build/pdoom.pbw'
prefix = sys.argv[2] if len(sys.argv) > 2 else 'smoke'
LOG = Path('work/smoke.log')


def run(*args):
    subprocess.run(cmd + list(args), check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', 'emery', *args)


def snap(name):
    run('screenshot', '--emulator', 'emery', '--no-open', f'work/{prefix}-{name}.png')


run('install', '--emulator', 'emery', pbw)
run('emu-accel', '--emulator', 'emery', 'gravity-z')
time.sleep(2)
with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        btn('click', 'select')          # New game
        time.sleep(1.5)
        snap('start')
        for i in range(3):              # look around: fire + down turns right
            btn('push', 'select')
            btn('--duration', '350', 'click', 'down')
            btn('release', 'select')
            time.sleep(0.4)
            snap(f'turn{i}')
        btn('push', 'select')
        btn('--duration', '350', 'click', 'down')
        btn('release', 'select')
        btn('--duration', '2500', 'click', 'up')
        time.sleep(0.5)
        snap('walk')
        time.sleep(3.5)
    finally:
        log.terminate()
        log.wait(timeout=5)
text = LOG.read_text(errors='replace')
errors = [l for l in text.splitlines() if re.search(r'FATAL|fault|Invalid', l)]
print('\n'.join(l for l in text.splitlines() if re.search(r'state |pos |FATAL|fault|Invalid|Zone', l))[-2000:])
print('ERRORS' if errors else 'no engine errors')
