import asyncio
import sys
import time
import bleak

#LUX_SERVICE_UUID = "e552241e-773c-1246-987d-8417d304edb5"
#LUX_CHAR_UUID    = "e552241e-773c-1246-987d-8417d304edb6"

ESS_SERVICE_UUID = "181a"
TEMP_CHAR_UUID   = "2a6e"
PRESS_CHAR_UUID  = "2a6d"
HUMID_CHAR_UUID  = "2a6f"
LUX_CHAR_UUID    = "2afb"

async def main(address):
    """Connect to device and read its lux Level characteristic."""
    try:
        async with bleak.BleakClient(address) as client:
            while(1):
                try:
                    ess_service = client.services.get_service(ESS_SERVICE_UUID)
                except AttributeError as err:
                    print("Unable to get service with UUID of {}".format(ESS_SERVICE_UUID))
                    return

                try:
                    lux_level_characteristic = (ess_service.get_characteristic(LUX_CHAR_UUID))
                    temp_level_characteristic = (ess_service.get_characteristic(TEMP_CHAR_UUID))
                    press_level_characteristic = (ess_service.get_characteristic(PRESS_CHAR_UUID))
                    humid_level_characteristic = (ess_service.get_characteristic(HUMID_CHAR_UUID))
                    
                    lux_level = await client.read_gatt_char(lux_level_characteristic)
                    temp_level = await client.read_gatt_char(temp_level_characteristic)
                    press_level = await client.read_gatt_char(press_level_characteristic)
                    humid_level = await client.read_gatt_char(humid_level_characteristic)
                    
                except AttributeError as err:
                    print("Unable to get characteristic")
                    print("Trying another approach...")
                    lux_level = await client.read_gatt_char(LUX_CHAR_UUID)
                    temp_level = await client.read_gatt_char(TEMP_CHAR_UUID)
                    press_level = await client.read_gatt_char(PRESS_CHAR_UUID)
                    humid_level = await client.read_gatt_char(HUMID_CHAR_UUID)
                    return
		
                print("Lux Level: {}".format(int.from_bytes(lux_level, byteorder='little', signed=False)))
                print("Temperature Level: {} deg C".format(int.from_bytes(temp_level, byteorder='little', signed=False)))
                print("Pressure Level: {} hPa".format(int.from_bytes(press_level, byteorder='little', signed=False)))
                print("Humid Level: {} %".format(int.from_bytes(humid_level, byteorder='little', signed=False)))
                print("")
                time.sleep(5)

    except asyncio.exceptions.TimeoutError:
        print(f"Can’t connect to device {address}.")


if __name__ == "__main__":
    if len(sys.argv) == 2:
        address = sys.argv[1]
        asyncio.run(main(address))
else:
    print("Please specify the Bluetooth address.")
