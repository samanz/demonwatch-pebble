"""Quick emulator look at a map: install, start a new game, take screenshots
while turning (by tilting the simulated watch), walk forward, press use (opens
a door ahead) and look through, then report engine errors and the zone/heap
figures from the log. The emulator presses one button at a time.

Usage: [EMULATOR=gabbro] python3 tools/smoke_map.py [build/pdoom.pbw] [name-prefix]
Screenshots go to work/<prefix>-*.png.
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
EMU = os.environ.get('EMULATOR', 'emery')   # or gabbro (Round 2)
pbw = sys.argv[1] if len(sys.argv) > 1 else 'build/pdoom.pbw'
prefix = sys.argv[2] if len(sys.argv) > 2 else 'smoke'
LOG = Path('work/smoke.log')


def run(*args):
    subprocess.run(cmd + list(args), check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', EMU, *args)


def snap(name):
    run('screenshot', '--emulator', EMU, '--no-open', f'work/{prefix}-{name}.png')


run('emu-accel', '--emulator', EMU, 'gravity-z')
with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', EMU], stdout=f, stderr=f)
    try:
        time.sleep(2)
        run('install', '--emulator', EMU, pbw)
        for _ in range(30):             # the title logs whether Continue is offered
            m = re.search(r'title: continue (\d)', LOG.read_text(errors='replace'))
            if m:
                break
            time.sleep(0.5)
        time.sleep(0.5)
        if m and m.group(1) == '1':
            btn('click', 'down')        # skip Continue
        btn('click', 'select')          # New game -> difficulty
        btn('click', 'select')          # last used difficulty
        time.sleep(1.5)
        snap('start')
        btn('--duration', '2500', 'click', 'up')
        time.sleep(0.5)
        snap('walk')
        btn('click', 'back')            # use: open the door ahead, if any
        time.sleep(1.5)
        snap('use')
        btn('--duration', '1200', 'click', 'up')
        time.sleep(0.3)
        snap('through')
        # Emulator tilt motions leave a net turn, so look around last.
        for i, motion in enumerate(('tilt-left', 'tilt-right')):
            run('emu-accel', '--emulator', EMU, motion)
            time.sleep(0.6)
            run('emu-accel', '--emulator', EMU, 'gravity-z')
            time.sleep(0.3)
            snap(f'turn{i}')
        time.sleep(3.5)
    finally:
        log.terminate()
        log.wait(timeout=5)
text = LOG.read_text(errors='replace')
errors = [l for l in text.splitlines() if re.search(r'FATAL|fault|Invalid', l)]
print('\n'.join(l for l in text.splitlines() if re.search(r'state |pos |FATAL|fault|Invalid|Zone', l))[-2000:])
print('ERRORS' if errors else 'no engine errors')
