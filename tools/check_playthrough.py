import subprocess,sys,time
from pathlib import Path
cmd=[sys.executable,'-u','tools/run_pebble.py']
# Relaunch app cleanly
subprocess.run(cmd+['install','--emulator','emery','build/pdoom.pbw'],check=True,timeout=30,stdout=subprocess.DEVNULL)
time.sleep(2)

with open('work/complete-playtest.log','w') as f:
 log=subprocess.Popen(cmd+['logs','--emulator','emery'],stdout=f,stderr=f)
 try:
  def btn(*args): subprocess.run(cmd+['emu-button','--emulator','emery']+list(args),check=True,timeout=20,stdout=subprocess.DEVNULL)
  def snap(name): subprocess.run(cmd+['screenshot','--emulator','emery','--no-open','work/'+name+'.png'],check=True,timeout=20,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
  # Start game from menu
  btn('click','select');time.sleep(1)
  # Sector 0 -> Door 1
  # Walk holds are generous: walls and closed doors stop the player.
  btn('--duration','2500','click','up')
  btn('click','back')
  time.sleep(1)
  # Room 1 (Courtyard) combat
  btn('push','select')
  time.sleep(2);snap('enemy-room1')
  time.sleep(6);btn('release','select')
  snap('after-fight-room1')
  # Advance across Courtyard to Door 2
  btn('click','back');time.sleep(1)
  btn('--duration','4500','click','up')
  btn('click','back');time.sleep(1)
  # Room 2 (Tech Lab) combat
  btn('push','select');time.sleep(8);btn('release','select')
  snap('fight-room2')
  # Advance across Tech Lab to Door 3
  btn('click','back');time.sleep(1)
  btn('--duration','4500','click','up')
  btn('click','back');time.sleep(1)
  # Room 3 (Exit Sanctum) combat
  btn('push','select');time.sleep(8);btn('release','select')
  snap('fight-room3')
  # Advance to Exit Switch wall at x=2368
  btn('click','back');time.sleep(1)
  btn('--duration','5000','click','up')
  time.sleep(0.5)
  snap('exit')
  # Activate Exit Switch
  btn('click','back');time.sleep(1.0)
  snap('restart')
  btn('--repeat','2','--interval','80','click','back')
  time.sleep(.5);snap('pause')
  btn('click','down');btn('click','select');time.sleep(.7);snap('pause-restart')
 finally:
  try: btn("release","select")
  finally: log.terminate();log.wait(timeout=5)
report=Path('work/complete-playtest.log').read_text(errors='replace')
print('\n'.join(x for x in report.splitlines() if 'pkjs>' not in x))
assert 'FATAL' not in report and 'Invalid lump' not in report, 'Engine error during playthrough'
assert 'kills' in report, 'Combat rooms must be engaged'
assert 'pos 128,256 hp100 ammo50 kills0' in report, 'Must start with initial player state'
assert 'state 1 map 1' in report, 'Exit switch must complete the level'
print('PASS: expanded playthrough completed, no engine errors; inspect exit/pause screenshots for UI state')
