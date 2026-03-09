import asyncio
import os
import time
from bleak import BleakClient

HEART_RATE_MEASUREMENT_CHAR_UUID = "00002a37-0000-1000-8000-00805f9b34fb"
MAX_DEVICES = 4
DEVICES_FILE = "devices.txt"

connected_clients = {}         # address -> BleakClient
device_file_map = {}           # address -> slot number
last_file_mtime = 0            # last modification time of devices.txt

def parse_heart_rate(data: bytearray) -> int | None:
    if not data or len(data) < 2:
        return None
    flags = data[0]
    hr_format = flags & 0x01
    if hr_format == 0:
        return data[1]
    else:
        return int.from_bytes(data[1:3], byteorder='little')

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

def remove_device_from_file(address, filename=DEVICES_FILE):
    try:
        with open(filename, "r") as f:
            lines = f.readlines()
        with open(filename, "w") as f:
            for line in lines:
                if line.strip() != address:
                    f.write(line)
        print(f"🧹 Usunięto {address} z {filename}")
    except Exception as e:
        print(f"⚠️ Błąd przy usuwaniu {address} z pliku: {e}")

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
        connected_clients.pop(address, None)
        slot = device_file_map.pop(address, None)
        if slot:
            with open(f"HR{slot}.txt", "w") as f:
                f.write("")
        remove_device_from_file(address)

async def connect_to_device(address, slot):
    try:
        client = BleakClient(address)
        await client.connect()

        if not client.is_connected:
            print(f"❌ Nie udało się połączyć z {address}")
            return

        print(f"🔗 Połączono z {address} (slot HR{slot})")
        await client.start_notify(
            HEART_RATE_MEASUREMENT_CHAR_UUID,
            make_notification_handler(address)
        )

        connected_clients[address] = client
        device_file_map[address] = slot
        asyncio.create_task(keep_alive(client, address))

    except Exception as e:
        print(f"⚠️ Błąd przy połączeniu z {address}: {e}")
        remove_device_from_file(address)

async def monitor_devices_file():
    global last_file_mtime

    while True:
        await asyncio.sleep(10)

        if len(connected_clients) >= MAX_DEVICES:
            continue

        if not os.path.exists(DEVICES_FILE):
            continue

        mtime = os.path.getmtime(DEVICES_FILE)
        if mtime == last_file_mtime:
            continue  # brak zmian

        last_file_mtime = mtime

        with open(DEVICES_FILE, "r") as f:
            addresses = [line.strip() for line in f if line.strip()]

        new_addresses = [addr for addr in addresses if addr not in connected_clients]

        for addr in new_addresses:
            if len(connected_clients) >= MAX_DEVICES:
                break
            slot = len(connected_clients) + 1
            await connect_to_device(addr, slot)

async def main():
    for i in range(1, MAX_DEVICES + 1):
        open(f"HR{i}.txt", "w").close()

    await monitor_devices_file()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("⛔ Zakończono program przez Ctrl+C")
