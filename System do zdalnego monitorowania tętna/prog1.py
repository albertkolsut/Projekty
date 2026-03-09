import asyncio
import datetime
import requests
from bleak import BleakScanner, BleakClient

HEART_RATE_SERVICE_UUID = "0000180d-0000-1000-8000-00805f9b34fb"
HEART_RATE_MEASUREMENT_CHAR_UUID = "00002a37-0000-1000-8000-00805f9b34fb"
MAX_DEVICES = 4

found_devices = {}
device_file_map = {}

def detection_callback(device, advertisement_data):
    global found_devices, device_file_map
    service_uuids = advertisement_data.service_uuids or []
    if HEART_RATE_SERVICE_UUID.lower() in [uuid.lower() for uuid in service_uuids]:
        if device.address not in found_devices and len(found_devices) < MAX_DEVICES:
            index = len(found_devices) + 1
            print(f"Znaleziono HR: {device.name} ({device.address}) -> HR{index}.txt")
            found_devices[device.address] = device
            device_file_map[device.address] = index

def parse_heart_rate(data: bytearray) -> int | None:
    if not data or len(data) < 2:
        return None
    flags = data[0]
    hr_format = flags & 0x01
    if hr_format == 0:
        return data[1] if len(data) >= 2 else None
    else:
        return int.from_bytes(data[1:3], byteorder='little') if len(data) >= 3 else None

def make_notification_handler(address):
    def handler(sender, data):
        heart_rate = parse_heart_rate(data)
        if heart_rate is not None:
            print(f"{address} -> {heart_rate} bpm")
            slot = device_file_map.get(address)
            if slot:
                with open(f"HR{slot}.txt", "w") as f:
                    f.write(f"{address} - {heart_rate} bpm")
        else:
            print(f"{address} -> Nieprawidłowe dane")
    return handler

async def keep_alive(client, address):
    try:
        while client.is_connected:
            await asyncio.sleep(1)
    except Exception as e:
        print(f"Błąd w keep_alive {address}: {e}")
    finally:
        print(f"❌ Rozłączono {address}")
        try:
            await client.stop_notify(HEART_RATE_MEASUREMENT_CHAR_UUID)
            await client.disconnect()
        except:
            pass

async def main():
    for i in range(1, MAX_DEVICES + 1):
        open(f"HR{i}.txt", "w").close()
    global found_devices, device_file_map

    while True:
        found_devices.clear()
        device_file_map.clear()

        scanner = BleakScanner()
        scanner.register_detection_callback(detection_callback)

        print("🔍 Skanowanie BLE (10s)...")
        await scanner.start()
        await asyncio.sleep(10)
        await scanner.stop()

        if not found_devices:
            print("❌ Nie znaleziono urządzeń. Restart za 5s...")
            await asyncio.sleep(5)
            continue

        print(f"✅ Znaleziono {len(found_devices)} urządzeń. Łączenie po kolei...")

        tasks = []

        for device in found_devices.values():
            try:
                client = BleakClient(device.address)
                await client.connect()

                if not client.is_connected:
                    print(f"❌ Nie udało się połączyć z {device.address}")
                    continue

                print(f"🔗 Połączono z {device.name} ({device.address})")

                await client.start_notify(
                    HEART_RATE_MEASUREMENT_CHAR_UUID,
                    make_notification_handler(device.address)
                )

                tasks.append(asyncio.create_task(keep_alive(client, device.address)))

            except Exception as e:
                print(f"⚠️ Błąd przy połączeniu z {device.address}: {e}")

        if not tasks:
            print("⚠️ Nie udało się połączyć z żadnym urządzeniem. Restart za 5s...")
            await asyncio.sleep(5)
            continue

        await asyncio.wait(tasks, return_when=asyncio.ALL_COMPLETED)
        print("🔁 Wszystkie urządzenia odłączone. Restart za 5s...")
        await asyncio.sleep(5)

# Start programu
if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("⛔ Zakończono program przez Ctrl+C")
