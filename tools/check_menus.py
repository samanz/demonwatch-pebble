"""Exercise pause/settings and persistence through a relaunch; inspect screenshots."""
import subprocess,sys,time
cmd=[sys.executable,'tools/run_pebble.py']
def run(*args): subprocess.run(cmd+list(args),check=True,timeout=30,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
def btn(*args): run('emu-button','--emulator','emery',*args)
def snap(name): run('screenshot','--emulator','emery','--no-open','work/'+name+'.png')
btn('--repeat','2','--interval','80','click','back')
btn('click','down');btn('click','down');btn('click','select')
btn('click','select');btn('click','down');btn('click','select');snap('settings-changed')
btn('--duration','1500','click','back')
run('install','--emulator','emery');time.sleep(.5);snap('title')
btn('click','down');btn('click','select');snap('settings-persisted')
# Restore normal sensitivity and direction for the delivery build's emulator session.
btn('click','select');btn('click','select');btn('click','down');btn('click','select')
btn('click','back');btn('click','down');btn('click','down');btn('click','select');snap('controls')
btn('click','back')
print('Menu navigation and persistence screenshots captured.')
