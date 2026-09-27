import os
import glob
import subprocess

for deb in glob.glob("/tmp/debs/*.deb"):
    print("Extracting:", deb)
    subprocess.call(["dpkg-deb", "-x", deb, "/home/sam/.local"])
