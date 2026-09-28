"""Emulator check of the E1M4 finale: when the Baron dies, the stone seal
(tag 666) sinks and the player can reach the exit room.

Uses the playtest build: holding Down 2 s warps to the next map, holding Up
4 s kills every monster. Warps to E1M4, kills the Barons from the supply
room, opens the door, walks north and checks the player gets past y = 1024
(the seal's north edge).
Usage: python3 tools/check_boss.py   (after tools/check_maps.py built build-playtest/)
"""
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
LOG = Path('work/check-boss.log')


def run(*args):
    subprocess.run(cmd + list(args), check=True, timeout=30, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def btn(*args):
    run('emu-button', '--emulator', 'emery', *args)


def log_text():
    return LOG.read_text(errors='replace')


def wait(pattern, seconds=20, after=0):
    end = time.time() + seconds
    while time.time() < end:
        if re.search(pattern, log_text()[after:]):
            return True
        time.sleep(0.5)
    return False


run('emu-accel', '--emulator', 'emery', 'gravity-z')
with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        time.sleep(2)
        run('install', '--emulator', 'emery', 'build-playtest/pdoom.pbw')
        wait(r'title: continue')
        time.sleep(0.5)
        if 'continue 1' in log_text():
            btn('click', 'down')
        btn('click', 'select')
        btn('click', 'select')
        for n in (1, 2, 3):
            if not wait(rf'state 0 map {n}\b'):
                sys.exit(f'FAIL: map {n} did not load')
            time.sleep(1)
            mark = len(log_text())
            btn('--duration', '2600', 'click', 'down')
            wait(rf'state 1 map {n}\b', after=mark)
            time.sleep(0.5)
            btn('click', 'select')
        if not wait(r'state 0 map 4\b'):
            sys.exit('FAIL: E1M4 did not load')
        time.sleep(1)
        btn('--duration', '4500', 'click', 'up')     # walk to the door; at 4 s kill everything
        btn('click', 'back')                          # playtest: tap action = open the door
        time.sleep(1.5)
        btn('--duration', '4500', 'click', 'up')     # through the arena and the lowered seal
        time.sleep(3.5)
        run('screenshot', '--emulator', 'emery', '--no-open', 'work/boss-exit.png')
    finally:
        log.terminate()
        log.wait(timeout=5)

text = log_text()
ys = [int(y) for y in re.findall(r'pos -?\d+,(-?\d+) ', text[text.rfind('state 0 map 4'):])]
print('E1M4 positions (y):', ys)
if 'PLAYTEST kill all' not in text:
    sys.exit('FAIL: the kill-all trigger did not fire')
if re.search(r'FATAL|App fault', text):
    sys.exit('FAIL: engine error')
if not ys or max(ys) <= 1024:
    sys.exit('FAIL: the seal did not open (player never got north of y = 1024)')
print('PASS: Baron killed, seal lowered, player reached the exit room')
