import asyncio
import sys
import bleak

#LUX_SERVICE_UUID = "e552241e-773c-1246-987d-8417d304edb5"
#LUX_CHAR_UUID    = "e552241e-773c-1246-987d-8417d304edb6"

TEMP_CHAR_UUID   = "00002a6e-0000-1000-8000-00805f9b34fb"
PRESS_CHAR_UUID  = "00002a6d-0000-1000-8000-00805f9b34fb"
HUMID_CHAR_UUID  = "00002a6f-0000-1000-8000-00805f9b34fb"
LUX_CHAR_UUID    = "00002afb-0000-1000-8000-00805f9b34fb"

def lux_level_changed(handle: int, data: bytearray):
    """ Show the lux level """
    print("Lux Level: {} lumens".format(int.from_bytes(data, byteorder='little', signed=True)))

def temperature_level_changed(handle: int, data: bytearray):
    """ Show the temperature level """
    print("Temperature Level: {} degC".format(int.from_bytes(data, byteorder='little', signed=True)))
    
def humidity_level_changed(handle: int, data: bytearray):
    """ Show the humidity level """
    print("Humidity Level: {}%".format(int.from_bytes(data, byteorder='little', signed=True)))
    print("")

def pressure_level_changed(handle: int, data: bytearray):
    """ Show the pressure level """
    print("Pressure Level: {} hPa".format(int.from_bytes(data, byteorder='little', signed=True)))

async def main(address):
    """Connect to device and subscribe to its Lux Level notificatins."""
    try:
        async with bleak.BleakClient(address) as client:
            print("Connected to address: {}".format(address))
            await client.start_notify(LUX_CHAR_UUID, lux_level_changed)
            await client.start_notify(TEMP_CHAR_UUID, temperature_level_changed)
            await client.start_notify(PRESS_CHAR_UUID, pressure_level_changed)
            await client.start_notify(HUMID_CHAR_UUID, humidity_level_changed)
            print("Notifications started...")

            while(1):
                await asyncio.sleep(5)

    except asyncio.exceptions.TimeoutError:
        print(f"Can’t connect to device {address}.")


if __name__ == "__main__":
    if len(sys.argv) == 2:
        address = sys.argv[1]
        asyncio.run(main(address))
else:
    print("Please specify the Bluetooth address.")
