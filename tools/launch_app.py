import uuid
import json
from libpebble2.communication import PebbleConnection
from libpebble2.communication.transports.websocket import WebsocketTransport
from libpebble2.protocol.apps import AppRunState, AppRunStateStart, AppRunStateStop

def get_pypkjs_port():
    with open('/tmp/pb-emulator.json') as f:
        data = json.load(f)
        return data['emery']['4.33.1']['pypkjs']['port']

def main():
    port = get_pypkjs_port()
    print(f"Connecting to pypkjs on port {port}...")
    transport = WebsocketTransport(f"ws://localhost:{port}/")
    pebble = PebbleConnection(transport)
    pebble.connect()
    pebble.run_async()

    app_uuid = uuid.UUID('d00364e1-0e11-4000-8000-000000000001')
    print(f"Sending AppRunStateStart for {app_uuid}...")
    pebble.send_packet(AppRunState(data=AppRunStateStart(uuid=app_uuid)))
    print("Sent launch command!")

if __name__ == '__main__':
    main()
