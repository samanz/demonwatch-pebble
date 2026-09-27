import subprocess,sys,time
from pathlib import Path
cmd=[sys.executable,'-u','tools/run_pebble.py']
with open('work/playtest.log','w') as f:
 log=subprocess.Popen(cmd+['logs','--emulator','emery'],stdout=f,stderr=f)
 try:
  def btn(*args): subprocess.run(cmd+['emu-button','--emulator','emery']+list(args),check=True)
  btn('--duration','1600','click','up')
  btn('click','back')
  time.sleep(1)
  btn('--duration','1300','click','up')
  btn('--duration','3000','click','select')
  subprocess.run(cmd+['screenshot','--emulator','emery','--no-open','work/combat.png'],check=True)
 finally:
  log.terminate();log.wait(timeout=5)
print(Path('work/playtest.log').read_text())
