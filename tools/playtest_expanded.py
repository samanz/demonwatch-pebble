import subprocess
import sys
import time

cmd = ['python3', 'tools/run_pebble.py']

def btn(*args):
    subprocess.run(cmd + ['emu-button', '--emulator', 'emery'] + list(args), check=True, timeout=20)

def snap(name):
    subprocess.run(cmd + ['screenshot', '--emulator', 'emery', '--no-open', f'work/{name}.png'], check=True, timeout=20)

print("Advancing to Door 2...")
btn('--duration', '3000', 'click', 'up')
time.sleep(0.5)

print("Opening Door 2...")
btn('click', 'back')
time.sleep(1.0)
snap('door2_open')

print("Shooting enemies in Tech Lab...")
btn('push', 'select')
time.sleep(4.0)
btn('release', 'select')
time.sleep(0.5)
snap('tech_lab_fight')

print("Advancing to Door 3...")
btn('--duration', '3200', 'click', 'up')
time.sleep(0.5)

print("Opening Door 3...")
btn('click', 'back')
time.sleep(1.0)
snap('door3_open')

print("Shooting enemies in Exit Sanctum...")
btn('push', 'select')
time.sleep(4.0)
btn('release', 'select')
time.sleep(0.5)
snap('sanctum_fight')

print("Advancing to Exit Switch...")
btn('--duration', '2800', 'click', 'up')
time.sleep(0.5)
snap('exit_switch_wall')

print("Pressing Exit Switch...")
btn('click', 'select')
time.sleep(1.5)
snap('intermission_or_restart')
