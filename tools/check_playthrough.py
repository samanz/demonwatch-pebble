"""Play the arena in the Emery emulator from spawn to the exit switch.

Builds and installs the PDOOM_PLAYTEST variant (invulnerable player, in
build-playtest/) so the result depends on the engine and level flow, not on
how well the bot shoots. Steers by the player position the app logs every
105 tics: at each waypoint it walks, and when blocked it reopens the door
ahead and fights. Saves screenshots and the log in work/. Run from the
project root.
"""
import os
import re
import subprocess
import sys
import time
from pathlib import Path

cmd = [sys.executable, '-u', 'tools/run_pebble.py']
LOG = Path('work/complete-playtest.log')
# Player x when standing against door 1, door 2, door 3, and the exit wall.
WAYPOINTS = [(490, 'door1'), (1130, 'door2'), (1770, 'door3'), (2340, 'exit')]

build = subprocess.run(cmd + ['build'], env=dict(os.environ, PDOOM_PLAYTEST='1'),
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=600)
if build.returncode:
    sys.exit('Playtest build failed:\n' + build.stdout[-3000:])
subprocess.run(cmd + ['install', '--emulator', 'emery', 'build-playtest/pdoom.pbw'],
               check=True, timeout=30, stdout=subprocess.DEVNULL)
# Hold the simulated watch flat: tilt steering is on by default, and the
# emulator keeps whatever accelerometer state an earlier session left behind.
subprocess.run(cmd + ['emu-accel', '--emulator', 'emery', 'gravity-z'],
               check=True, timeout=20, stdout=subprocess.DEVNULL)
time.sleep(2)


def btn(*args):
    subprocess.run(cmd + ['emu-button', '--emulator', 'emery'] + list(args),
                   check=True, timeout=20, stdout=subprocess.DEVNULL)


def snap(name):
    subprocess.run(cmd + ['screenshot', '--emulator', 'emery', '--no-open', 'work/' + name + '.png'],
                   check=True, timeout=20, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def report():
    return LOG.read_text(errors='replace')


def player_x():
    xs = re.findall(r'pos (-?\d+),', report())
    return int(xs[-1]) if xs else 0


def player_hp():
    hps = re.findall(r'pos -?\d+,-?\d+ hp(-?\d+)', report())
    return int(hps[-1]) if hps else 100


def fight(seconds):
    btn('push', 'select')
    time.sleep(seconds)
    btn('release', 'select')


def advance(target, name, tries=6):
    for attempt in range(tries):
        btn('--duration', '3000', 'click', 'up')
        time.sleep(3.3)             # wait for the next position report
        if player_hp() <= 0:
            snap(f'{name}-died')
            raise AssertionError(f'Player died before {name}; last x={player_x()}')
        if player_x() >= target:
            return
        # Blocked: on alternate tries shoot whatever is in the way, then
        # reopen the door ahead (doors close ~4 s after opening) and walk
        # straight through. Use is never pressed before walking otherwise:
        # standing in an open door, it would close the door again.
        snap(f'{name}-blocked-{attempt}')
        if attempt % 2:
            fight(4)
        btn('click', 'back')
        time.sleep(1.2)
    raise AssertionError(f'Stuck before {name} (x>={target}); last x={player_x()}')


with LOG.open('w') as f:
    log = subprocess.Popen(cmd + ['logs', '--emulator', 'emery'], stdout=f, stderr=f)
    try:
        btn('click', 'select')      # New game
        time.sleep(1)
        for i, (target, name) in enumerate(WAYPOINTS):
            advance(target, name)
            if name != 'exit':
                btn('click', 'back')    # open the door and clear the room beyond it
                time.sleep(1)
                fight(8)
                snap(f'fight-room{i + 1}')
        snap('exit')
        btn('click', 'back')        # exit switch
        time.sleep(1.5)
        snap('restart')             # level-clear screen
        btn('--repeat', '2', '--interval', '80', 'click', 'back')
        time.sleep(.5)
        snap('pause')
        btn('click', 'down')
        btn('click', 'select')
        time.sleep(.7)
        snap('pause-restart')
        time.sleep(3.3)
    finally:
        try:
            btn('release', 'select')
        finally:
            log.terminate()
            log.wait(timeout=5)

text = report()
print('\n'.join(x for x in text.splitlines() if 'pkjs>' not in x))
assert 'PLAYTEST build' in text, 'Expected the invulnerable playtest build to be running'
assert 'FATAL' not in text and 'Invalid lump' not in text and 'App fault' not in text, 'Engine error'
assert 'pos 128,256 hp100 ammo50 kills0' in text, 'Must start with initial player state'
assert re.search(r'kills[1-9]', text), 'Combat rooms must be engaged'
assert 'state 1 map 1' in text, 'Exit switch must complete the level'
print('PASS: spawn to exit switch, level completed, no engine errors')
