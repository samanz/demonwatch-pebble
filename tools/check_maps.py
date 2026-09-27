"""Visit every map in the emulator using the playtest build's level warp.

Builds the PDOOM_PLAYTEST variant (build-playtest/, invulnerable player;
holding Down for 2 s exits the level), starts a new game, and for each map:
waits for it to load, looks around by tilting the simulated watch (default
tilt steering) and walks a little (screenshots in work/map<N>-*.png), warps to
the exit, checks the LEVEL CLEAR screen and presses Select for the next map.
The emulator presses one button at a time, so no chorded input is used.
Fails on engine errors, missing maps, or low zone memory.

Usage: python3 tools/check_maps.py [number of maps, default 3]
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
LOG = Path('work/check-maps.log')
MAPS = int(sys.argv[1]) if len(sys.argv) > 1 else 3
MIN_ZONE = 1024   # bytes of zone that must stay free


def run(*args, timeout=30):
    subprocess.run(cmd + list(args), check=True, timeout=timeout,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', 'emery', *args)


def snap(name):
    run('screenshot', '--emulator', 'emery', '--no-open', f'work/{name}.png')


def log_text():
    return LOG.read_text(errors='replace')


def wait_for(pattern, seconds):
    end = time.time() + seconds
    while time.time() < end:
        if re.search(pattern, log_text()):
            return True
        time.sleep(0.5)
    return False


build = subprocess.run(cmd + ['build'], env=dict(os.environ, PDOOM_PLAYTEST='1'),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
if build.returncode:
    sys.exit('Playtest build failed:\n' + build.stdout[-3000:])
run('install', '--emulator', 'emery', 'build-playtest/pdoom.pbw')
run('emu-accel', '--emulator', 'emery', 'gravity-z')
time.sleep(2)

failures = []
with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        time.sleep(1)
        btn('click', 'select')                      # New game
        for n in range(1, MAPS + 1):
            if not wait_for(rf'state 0 map {n}\b', 15):
                failures.append(f'map {n} did not load')
                break
            time.sleep(1)
            snap(f'map{n}-start')
            for i, motion in enumerate(('tilt-left', 'tilt-right')):
                run('emu-accel', '--emulator', 'emery', motion)
                time.sleep(0.6)
                run('emu-accel', '--emulator', 'emery', 'gravity-z')
                time.sleep(0.3)
                snap(f'map{n}-look{i}')
            btn('--duration', '1500', 'click', 'up')
            snap(f'map{n}-walk')
            time.sleep(3.2)                         # let a position/zone report arrive
            btn('--duration', '2600', 'click', 'down')   # playtest warp
            if not wait_for(rf'state 1 map {n}\b', 10):
                failures.append(f'map {n}: warp did not complete the level')
                break
            time.sleep(0.8)
            snap(f'map{n}-clear')
            btn('click', 'select')                  # next level, or new game after the last
        time.sleep(1)
    finally:
        log.terminate()
        log.wait(timeout=5)

text = log_text()
for line in text.splitlines():
    if re.search(r'state |new game|zone|FATAL|fault', line) and 'pkjs' not in line:
        print(line)
if re.search(r'FATAL|App fault|Invalid lump', text):
    failures.append('engine error in log')
if 'PLAYTEST build' not in text:
    failures.append('the playtest build was not running')
zones = [int(z) for z in re.findall(r'zone(\d+)', text)]
if zones and min(zones) < MIN_ZONE:
    failures.append(f'zone fell to {min(zones)} B free')
print(f'lowest zone free: {min(zones) if zones else "n/a"} B')
if failures:
    sys.exit('FAIL: ' + '; '.join(failures))
print(f'PASS: {MAPS} maps loaded, rendered and completed in order')
