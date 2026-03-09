import asyncio
from bleak import BleakScanner
import os

HEART_RATE_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb"
MAX_DEVICES = 4
OUTPUT_FILE = "devices.txt"

existing_addresses = set()



def load_existing_addresses():
    if os.path.exists(OUTPUT_FILE):
        with open(OUTPUT_FILE, "r") as f:
            return set(line.strip() for line in f if line.strip())
    return set()

def detection_callback(device, advertisement_data):
    global existing_addresses
    service_uuids = advertisement_data.service_uuids or []
    if HEART_RATE_SERVICE_UUID.lower() in [uuid.lower() for uuid in service_uuids]:
        if device.address not in existing_addresses:
            print(f"Znaleziono HR: {device.name} ({device.address})")
            with open(OUTPUT_FILE, "a") as f:
                f.write(device.address + "\n")
            existing_addresses.add(device.address)

async def main():
    global existing_addresses

    # 🧹 Wyczyść plik na początku działania
    with open(OUTPUT_FILE, "w") as f:
        pass

    existing_addresses = set()

    while True:
        scanner = BleakScanner()
        scanner.register_detection_callback(detection_callback)

        print("🔍 Skanowanie BLE (10s)...")
        await scanner.start()
        await asyncio.sleep(10)
        await scanner.stop()

        await asyncio.sleep(5)

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("⛔ Zakończono skanowanie")
