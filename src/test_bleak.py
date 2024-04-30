"""
Since our ble_test_service.c file uses BT_GATT_CHRC_NOTIFY and bt_gatt_notify(),
  we have to use bleak's start_notify() feature instead of the serial GATT read/write 
  features.

"""


import asyncio
from bleak import BleakClient
import bleak

hw_address = "FF:EE:DE:AD:BE:EF"  # NOTE: replace with your hw_address
service_uuid = '00001523-1212-efde-1523-785fdeadbeef'  # NOTE: replace with your service uuid


async def print_notification(sender: bleak.discover, data: bytearray) -> None:
    print(f"received: {data}")


async def main(hw_address: str):
    try:
        client = BleakClient(hw_address)
        await client.connect()
        ptr_to_char = None
        our_service = [x for x in client.services if x.uuid == service_uuid]
        our_service = our_service[0]
        our_char = our_service.characteristics[0]

        await client.start_notify(our_char.uuid, print_notification)
        while True:
            print("hello")
            await asyncio.sleep(2)

    except Exception as e:
        print(e)
    finally:
        await client.disconnect()

asyncio.run(main(hw_address))
