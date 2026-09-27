import os
import subprocess
import sys

os.environ["PATH"] = "/home/sam/.local/share/pebble-sdk/SDKs/4.33.1/toolchain/arm-none-eabi/bin:/home/sam/.local/bin:" + os.environ.get("PATH", "")
os.environ["LD_LIBRARY_PATH"] = "/home/sam/.local/usr/lib/x86_64-linux-gnu:/home/sam/.local/usr/lib/x86_64-linux-gnu/pulseaudio:" + os.environ.get("LD_LIBRARY_PATH", "")

args = ["pebble"] + sys.argv[1:]
res = subprocess.call(args)
sys.exit(res)
