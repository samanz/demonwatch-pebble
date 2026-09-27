import sys
import traceback
from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.websocket import WebsocketTransport
from libpebble2.services.install import AppInstaller
from libpebble2.services.blobdb import BlobDBClient

def main():
    print("Checking build/pdoom.pbw...")
    import zipfile, json, struct
    with zipfile.ZipFile("build/pdoom.pbw") as z:
        print("Files in PBW:", z.namelist())
        appinfo = json.loads(z.read("appinfo.json").decode())
        print("AppInfo:", json.dumps(appinfo, indent=2))
        raw_bin = z.read("emery/pebble-app.bin")
        print("Raw bin length:", len(raw_bin))
        print("First 128 bytes:", raw_bin[:128])
        pos = raw_bin.find(b'PebbleApp')
        print("Position of 'PebbleApp' in raw_bin:", pos)
        if pos != -1:
            print("Bytes around PebbleApp:", raw_bin[pos:pos+128])

    # Connect to pypkjs via websocket
    print("Connecting to pypkjs websocket...")
    transport = WebsocketTransport("ws://localhost:35653/")
    pebble = PebbleConnection(transport)
    pebble.connect()
    pebble.run_async()

    print("Trying AppInstaller directly...")
    try:
        blobdb = BlobDBClient(pebble)
        installer = AppInstaller(pebble, "build/pdoom.pbw", blobdb_client=blobdb)
        installer.install()
        print("Install succeeded!")
    except Exception as e:
        print(f"Exception during AppInstaller.install: {type(e).__name__}: {e}")
        traceback.print_exc()

if __name__ == "__main__":
    main()
