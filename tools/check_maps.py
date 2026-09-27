"""Visit every map in the emulator using the playtest build's level warp,
and exercise checkpoints (Continue after quitting, Restart level).

Builds the PDOOM_PLAYTEST variant (build-playtest/, invulnerable player;
holding Down for 2 s exits the level) and plays through the episode:

  title -> New game -> difficulty -> map 1 -> warp -> map 2
  map 2: quit from the pause menu, relaunch, Continue -> map 2 again with the
         checkpoint restored -> warp -> map 3
  map 3: pause -> Restart level -> checkpoint restored -> warp -> YOU WIN
         (checkpoint deleted) -> Select -> new game on map 1

For each map it waits for the load, looks around by tilting the simulated
watch, walks a little and saves screenshots (work/map<N>-*.png). The emulator
presses one button at a time, so no chorded input is used. Fails on engine
errors, missing maps, checkpoint problems, or low zone memory.

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
failures = []


def run(*args, timeout=30):
    subprocess.run(cmd + list(args), check=True, timeout=timeout,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', 'emery', *args)


def snap(name):
    run('screenshot', '--emulator', 'emery', '--no-open', f'work/{name}.png')


def log_text():
    return LOG.read_text(errors='replace')


def wait_for(pattern, seconds, after=0):
    """Wait for a regex match in the log after character offset `after`."""
    end = time.time() + seconds
    while time.time() < end:
        m = re.search(pattern, log_text()[after:])
        if m:
            return m
        time.sleep(0.5)
    return None


def mark():
    return len(log_text())


def launch():
    """(Re)install and launch; return whether the title offers Continue."""
    start = mark()
    run('install', '--emulator', 'emery', 'build-playtest/pdoom.pbw')
    m = wait_for(r'title: continue (\d)', 15, start)
    if not m:
        sys.exit('FAIL: app did not reach the title menu')
    time.sleep(0.5)
    return m.group(1) == '1'


def look_around(n):
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


def warp(n):
    start = mark()
    btn('--duration', '2600', 'click', 'down')   # playtest warp
    if not wait_for(rf'state 1 map {n}\b', 10, start):
        failures.append(f'map {n}: warp did not complete the level')
        return False
    time.sleep(0.8)
    snap(f'map{n}-clear')
    return True


def pause_menu_pick(index):
    """Open the pause menu (double Back) and choose item `index` from the top."""
    btn('--repeat', '2', '--interval', '80', 'click', 'back')
    time.sleep(0.5)
    for _ in range(index):
        btn('click', 'down')
    btn('click', 'select')


build = subprocess.run(cmd + ['build'], env=dict(os.environ, PDOOM_PLAYTEST='1'),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
if build.returncode:
    sys.exit('Playtest build failed:\n' + build.stdout[-3000:])
run('emu-accel', '--emulator', 'emery', 'gravity-z')

with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        time.sleep(2)
        has_continue = launch()
        if has_continue:
            btn('click', 'down')            # skip Continue: start fresh
        btn('click', 'select')              # New game -> difficulty page
        time.sleep(0.4)
        snap('difficulty')
        start = mark()
        btn('click', 'select')              # last used difficulty
        n = 1
        while n <= MAPS:
            if not wait_for(rf'state 0 map {n}\b', 15, start):
                failures.append(f'map {n} did not load')
                break
            look_around(n)
            if n == 1:
                pause_menu_pick(1)          # Map
                time.sleep(0.5)
                snap('map1-automap')
                btn('click', 'back')        # map -> pause menu
                btn('click', 'back')        # pause -> game
                time.sleep(0.5)
            if n == 2 and MAPS >= 3:
                # Quit, relaunch, Continue: map 2 again from the checkpoint.
                pause_menu_pick(5)          # Quit
                time.sleep(1)
                if not launch():
                    failures.append('no Continue after quitting on map 2')
                    break
                start = mark()
                btn('click', 'select')      # Continue
                if not wait_for(r'checkpoint restored: map 2', 15, start):
                    failures.append('Continue did not restore the map 2 checkpoint')
                    break
                wait_for(r'state 0 map 2\b', 10, start)
                time.sleep(1)
                snap('map2-continued')
            if n == 3:
                start = mark()
                pause_menu_pick(2)          # Restart level
                if not wait_for(r'checkpoint restored: map 3', 10, start):
                    failures.append('Restart level did not restore the checkpoint')
                    break
                time.sleep(1)
                snap('map3-restarted')
            if not warp(n):
                break
            start = mark()
            btn('click', 'select')          # next level, or new game after the last
            n += 1
        if n > MAPS:
            if not wait_for(r'state 0 map 1\b', 15, start):
                failures.append('Select after the last map did not start a new game')
        time.sleep(1)
    finally:
        log.terminate()
        log.wait(timeout=5)

text = log_text()
for line in text.splitlines():
    if re.search(r'state |new game|checkpoint|title:|FATAL|fault', line) and 'pkjs' not in line:
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
print(f'PASS: {MAPS} maps loaded, rendered and completed in order; Continue and Restart level restored checkpoints')
