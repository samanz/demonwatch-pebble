"""Emulator check of the screen-tap action (use if something usable is ahead,
otherwise fire). The emulator has no touch input, so the playtest build maps a
single Back click to the tap action.

E1M1: walk to the closed door and tap (must open it, no shot), tap again at
the open door (must fire, not close it), then check the ammo count.
Usage: python3 tools/check_tap.py   (run from the project root)
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
LOG = Path('work/check-tap.log')


def run(*args):
    subprocess.run(cmd + list(args), check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', 'emery', *args)


def snap(name):
    run('screenshot', '--emulator', 'emery', '--no-open', f'work/{name}.png')


def reports():
    return re.findall(r'pos (-?\d+),(-?\d+) hp\d+ ammo(\d+)', LOG.read_text(errors='replace'))


build = subprocess.run(cmd + ['build'], env=dict(os.environ, PDOOM_PLAYTEST='1'),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
if build.returncode:
    sys.exit('Playtest build failed:\n' + build.stdout[-3000:])
run('emu-accel', '--emulator', 'emery', 'gravity-z')
with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        time.sleep(2)
        run('install', '--emulator', 'emery', 'build-playtest/pdoom.pbw')
        time.sleep(3)
        if 'continue 1' in LOG.read_text(errors='replace'):
            btn('click', 'down')
        btn('click', 'select')
        btn('click', 'select')
        time.sleep(1.5)
        btn('--duration', '2500', 'click', 'up')    # to the closed door
        time.sleep(3.5)
        before = reports()[-1]
        btn('click', 'back')                        # tap: door ahead -> use
        time.sleep(1.5)
        snap('tap-door')
        time.sleep(2.5)
        after_open = reports()[-1]
        btn('click', 'back')                        # tap at the open door -> fire
        time.sleep(3.5)
        snap('tap-fire')
        after_fire = reports()[-1]
    finally:
        log.terminate()
        log.wait(timeout=5)

print('at door:', before, ' after first tap:', after_open, ' after second tap:', after_fire)
failures = []
if int(after_open[2]) != int(before[2]):
    failures.append('the tap at a closed door fired instead of opening it')
if int(after_fire[2]) >= int(after_open[2]):
    failures.append('the tap at an open door did not fire')
if re.search(r'FATAL|App fault', LOG.read_text(errors='replace')):
    failures.append('engine error')
if failures:
    sys.exit('FAIL: ' + '; '.join(failures) + ' (see work/tap-door.png, work/tap-fire.png)')
print('PASS: tap opened the door, then fired through the open doorway')
