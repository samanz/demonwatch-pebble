import subprocess

p = subprocess.Popen(["python3", "tools/run_pebble.py", "repl", "--emulator", "emery"],
                     stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
out, err = p.communicate("print('Platform:', pebble.watch_platform)\nprint('Firmware:', pebble.firmware_version)\nexit()\n", timeout=5)
print("STDOUT:\n", out)
print("STDERR:\n", err)
