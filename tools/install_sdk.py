import os
import subprocess
import sys

os.environ["PATH"] = "/home/sam/.local/bin:" + os.environ.get("PATH", "")
print("Current PATH:", os.environ["PATH"])
try:
    npm_v = subprocess.check_output(["npm", "--version"], text=True).strip()
    print("npm version detected:", npm_v)
except Exception as e:
    print("Failed to run npm:", e)

res = subprocess.call(["/home/sam/.local/bin/pebble", "sdk", "install", "4.33.1"])
sys.exit(res)
