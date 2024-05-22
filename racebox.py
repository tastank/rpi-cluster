"""
Shamelessly stolen from https://github.com/hbldh/bleak/blob/develop/examples/uart_service.py

TODO: figure out why it frequently disconnects or throws bleak.exc.BleakDBusError (full traceback below) on start and stop them from happening.
Traceback (most recent call last):
  File "/home/pi/rpi-cluster/racebox.py", line 299, in <module>
    asyncio.run(RaceBoxData(sn).read_racebox())
  File "/usr/lib/python3.9/asyncio/runners.py", line 44, in run
    return loop.run_until_complete(main)
  File "/usr/lib/python3.9/asyncio/base_events.py", line 642, in run_until_complete
    return future.result()
  File "/home/pi/rpi-cluster/racebox.py", line 283, in read_racebox
    async with BleakClient(device, disconnected_callback=handle_disconnect) as client:
  File "/home/pi/.local/lib/python3.9/site-packages/bleak/__init__.py", line 570, in __aenter__
    await self.connect()
  File "/home/pi/.local/lib/python3.9/site-packages/bleak/__init__.py", line 615, in connect
    return await self._backend.connect(**kwargs)
  File "/home/pi/.local/lib/python3.9/site-packages/bleak/backends/bluezdbus/client.py", line 254, in connect
    assert_reply(reply)
  File "/home/pi/.local/lib/python3.9/site-packages/bleak/backends/bluezdbus/utils.py", line 20, in assert_reply
    raise BleakDBusError(reply.error_name, reply.body)
bleak.exc.BleakDBusError: [org.bluez.Error.Failed] Software caused connection abort

TODO: do something with the flags types other than just coercing them to int and shipping them along with the rest of the (potentially garbage, since we didn't check the flags) data
"""

import asyncio
import bleak
import json
import logging
import os
import sys
import time
import zmq

from enum import IntEnum
from itertools import count, takewhile
from typing import Iterator

from bleak import BleakClient, BleakScanner
from bleak.backends.characteristic import BleakGATTCharacteristic
from bleak.backends.device import BLEDevice
from bleak.backends.scanner import AdvertisementData

def send_zmq_json(socket, data):
    socket.send_json(data)

UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
UART_RX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
UART_TX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

PAYLOAD_OFFSET = 6
ITOW_OFFSET = PAYLOAD_OFFSET + 0
ITOW_BYTES = 4
YEAR_OFFSET = PAYLOAD_OFFSET + 4
YEAR_BYTES = 2
MONTH_OFFSET = PAYLOAD_OFFSET + 6
MONTH_BYTES = 1
DAY_OFFSET = PAYLOAD_OFFSET + 7
DAY_BYTES = 1
HOUR_OFFSET = PAYLOAD_OFFSET + 8
HOUR_BYTES = 1
MINUTE_OFFSET = PAYLOAD_OFFSET + 9
MINUTE_BYTES = 1
SECOND_OFFSET = PAYLOAD_OFFSET + 10
SECOND_BYTES = 1
VALIDITY_FLAGS_OFFSET = PAYLOAD_OFFSET + 11
VALIDITY_FLAGS_BYTES = 1
TIME_ACCURACY_OFFSET = PAYLOAD_OFFSET + 12
TIME_ACCURACY_BYTES = 4
NANOSECONDS_OFFSET = PAYLOAD_OFFSET + 16
NANOSECONDS_BYTES = 4
FIX_STATUS_OFFSET = PAYLOAD_OFFSET + 20
FIX_STATUS_BYTES = 1
FIX_STATUS_FLAGS_OFFSET = PAYLOAD_OFFSET + 21
FIX_STATUS_FLAGS_BYTES = 1
DATE_TIME_FLAGS_OFFSET = PAYLOAD_OFFSET + 22
DATE_TIME_FLAGS_BYTES = 1
NUMBER_OF_SVS_OFFSET = PAYLOAD_OFFSET + 23
NUMBER_OF_SVS_BYTES = 1
LONGITUDE_OFFSET = PAYLOAD_OFFSET + 24
LONGITUDE_BYTES = 4
LATITUDE_OFFSET = PAYLOAD_OFFSET + 28
LATITUDE_BYTES = 4
WGS_ALTITUDE_OFFSET = PAYLOAD_OFFSET + 32
WGS_ALTITUDE_BYTES = 4
MSL_ALTITUDE_OFFSET = PAYLOAD_OFFSET + 36
MSL_ALTITUDE_BYTES = 4
HORIZONTAL_ACCURACY_OFFSET = PAYLOAD_OFFSET + 40
HORIZONTAL_ACCURACY_BYTES = 4
VERTICAL_ACCURACY_OFFSET = PAYLOAD_OFFSET + 44
VERTICAL_ACCURACY_BYTES = 4
SPEED_OFFSET = PAYLOAD_OFFSET + 48
SPEED_BYTES = 4
HEADING_OFFSET = PAYLOAD_OFFSET + 42
HEADING_BYTES = 4
SPEED_ACCURACY_OFFSET = PAYLOAD_OFFSET + 56
SPEED_ACCURACY_BYTES = 4
HEADING_ACCURACY_OFFSET = PAYLOAD_OFFSET + 60
HEADING_ACCURACY_BYTES = 4
PDOP_OFFSET = PAYLOAD_OFFSET + 64
PDOP_BYTES = 2
LATLON_FLAGS_OFFSET = PAYLOAD_OFFSET + 66
LATLON_FLAGS_BYTES = 1
INPUT_VOLTAGE_OFFSET = PAYLOAD_OFFSET + 67
INPUT_VOLTAGE_BYTES = 1
GFORCE_X_OFFSET = PAYLOAD_OFFSET + 68
GFORCE_X_BYTES = 2
GFORCE_Y_OFFSET = PAYLOAD_OFFSET + 70
GFORCE_Y_BYTES = 2
GFORCE_Z_OFFSET = PAYLOAD_OFFSET + 72
GFORCE_Z_BYTES = 2
ROTATION_RATE_X_OFFSET = PAYLOAD_OFFSET + 74
ROTATION_RATE_X_BYTES = 2
ROTATION_RATE_Y_OFFSET = PAYLOAD_OFFSET + 76
ROTATION_RATE_Y_BYTES = 2
ROTATION_RATE_Z_OFFSET = PAYLOAD_OFFSET + 78
ROTATION_RATE_Z_BYTES = 2

DATE_TIME_CONFIRM_AVAIL_MASK = 0b00100000
DATE_VALID_MASK = 0b01000000
TIME_VALID_MASK = 0b10000000

class FixStatus(IntEnum):
    NO_FIX = 0
    FIX_2D = 2
    FIX_3D = 3

class RaceBoxData:
    def __init__(self):
        self.__init__(0)

    def __init__(self, serial_number):
        self.iTOW = 0
        self.year = 0
        self.month = 0
        self.day = 0
        self.hour = 0
        self.minute = 0
        self.second = 0
        self.validity_flags = None
        self.time_accuracy = None
        self.nanoseconds = 0
        self.fix_status = None
        self.fix_status_flags = None
        self.date_time_flags = None
        self.number_of_svs = 0
        self.longitude = 0
        self.latitude = 0
        self.wgs_altitude = 0
        self.wgs_altitude_ft = 0
        self.msl_altitude = 0
        self.msl_altitude_ft = 0
        self.horizontal_accuracy = 0
        self.vertical_accuracy = 0
        self.speed = 0
        self.speed_mph = 0
        self.heading = 0
        self.speed_accuracy = 0
        self.heading_accuracy = 0
        self.pdop = 0
        self.latlon_flags = None
        self.input_voltage = None
        self.gforce_x = 0
        self.gforce_y = 0
        self.gforce_z = 0
        self.rotation_rate_x = 0
        self.rotation_rate_y = 0
        self.rotation_rate_z = 0
        self.serial_number = serial_number

    def as_dict(self):
        return {
            "iTOW": self.iTOW,
            "year": self.year,
            "month": self.month,
            "day": self.day,
            "hour": self.hour,
            "minute": self.minute,
            "second": self.second,
            "validity_flags": self.validity_flags,
            "time_accuracy": self.time_accuracy,
            "nanoseconds": self.nanoseconds,
            "fix_status": self.fix_status,
            "fix_status_flags": self.fix_status_flags,
            "date_time_flags": self.date_time_flags,
            "number_of_svs": self.number_of_svs,
            "longitude": self.longitude,
            "latitude": self.latitude,
            "wgs_altitude_m": self.wgs_altitude,
            "wgs_altitude_ft": self.wgs_altitude_ft,
            "msl_altitude_m": self.msl_altitude,
            "msl_altitude_ft": self.msl_altitude_ft,
            "horizontal_accuracy": self.horizontal_accuracy,
            "vertical_accuracy": self.vertical_accuracy,
            "speed_ms": self.speed,
            "speed_mph": self.speed_mph,
            "heading": self.heading,
            "speed_accuracy_ms": self.speed_accuracy,
            "heading_accuracy": self.heading_accuracy,
            "pdop": self.pdop,
            "latlon_flags": self.latlon_flags,
            "input_voltage": self.input_voltage,
            "gforce_x": self.gforce_x,
            "gforce_y": self.gforce_y,
            "gforce_z": self.gforce_z,
            "rotation_rate_x": self.rotation_rate_x,
            "rotation_rate_y": self.rotation_rate_y,
            "rotation_rate_z": self.rotation_rate_z,
        }

    def set_serial_number(self, sn):
        self.serial_number = sn

    async def read_racebox(self, logger):
        context = zmq.Context()
        socket = context.socket(zmq.PUSH)
        socket.connect("tcp://localhost:9962")

        def match_racebox(device: BLEDevice, adv: AdvertisementData):
            # This assumes that the device includes the UART service UUID in the
            # advertising data. This test may need to be adjusted depending on the
            # actual advertising data supplied by the device.
            if "RaceBox" in device.name and str(self.serial_number) in device.name:# or device.address.lower() == "c8:cf:49:de:14:01":
                return True

            return False

        device = await BleakScanner.find_device_by_filter(match_racebox)

        if device is None:
            logger.error("no matching device found, you may need to edit match_racebox().")
            return
        logger.info("RaceBox found")

        def handle_disconnect(_: BleakClient):
            logger.warn("Device was disconnected, goodbye.")
            # cancelling all tasks effectively ends the program
            for task in asyncio.all_tasks():
                task.cancel()

        def verify_checksum(data):
            ck_a = 0
            ck_b = 0
            for i in range(2, len(data)-2):
                ck_a += data[i]
                ck_b += ck_a
            ck_a %= 256
            ck_b %= 256
            if data[-2] != ck_a or data[-1] != ck_b:
                return False
            else:
                return True

        def update_class_data(data):
            self.iTOW = int.from_bytes(data[ITOW_OFFSET:ITOW_OFFSET + ITOW_BYTES], "little")
            self.year = int.from_bytes(data[YEAR_OFFSET:YEAR_OFFSET + YEAR_BYTES], "little")
            # the one byte integer fields can be directly converted
            self.month = data[MONTH_OFFSET]
            self.day = data[DAY_OFFSET]
            self.hour = data[HOUR_OFFSET]
            self.minute = data[MINUTE_OFFSET]
            self.second = data[SECOND_OFFSET]
            self.validity_flags = data[VALIDITY_FLAGS_OFFSET]
            self.time_accuracy = int.from_bytes(data[TIME_ACCURACY_OFFSET:TIME_ACCURACY_OFFSET + TIME_ACCURACY_BYTES], "little")
            self.nanoseconds = int.from_bytes(data[NANOSECONDS_OFFSET:NANOSECONDS_OFFSET + NANOSECONDS_BYTES], "little", signed=True)
            self.fix_status = data[FIX_STATUS_OFFSET]
            self.fix_status_flags = data[FIX_STATUS_FLAGS_OFFSET]
            self.date_time_flags = data[DATE_TIME_FLAGS_OFFSET]
            self.number_of_svs = data[NUMBER_OF_SVS_OFFSET]
            self.longitude = int.from_bytes(data[LONGITUDE_OFFSET:LONGITUDE_OFFSET + LONGITUDE_BYTES], "little", signed=True)/1e7
            self.latitude = int.from_bytes(data[LATITUDE_OFFSET:LATITUDE_OFFSET + LATITUDE_BYTES], "little", signed=True)/1e7
            self.wgs_altitude = int.from_bytes(data[WGS_ALTITUDE_OFFSET:WGS_ALTITUDE_OFFSET + WGS_ALTITUDE_BYTES], "little", signed=True)/1e3
            self.wgs_altitude_ft = self.wgs_altitude*3.28084
            self.msl_altitude = int.from_bytes(data[MSL_ALTITUDE_OFFSET:MSL_ALTITUDE_OFFSET + MSL_ALTITUDE_BYTES], "little", signed=True)/1e3
            self.msl_altitude_ft = self.msl_altitude*3.28084
            self.horizontal_accuracy = int.from_bytes(data[HORIZONTAL_ACCURACY_OFFSET:HORIZONTAL_ACCURACY_OFFSET + HORIZONTAL_ACCURACY_BYTES], "little")
            self.vertical_accuracy = int.from_bytes(data[VERTICAL_ACCURACY_OFFSET:VERTICAL_ACCURACY_OFFSET + VERTICAL_ACCURACY_BYTES], "little")
            self.speed = int.from_bytes(data[SPEED_OFFSET:SPEED_OFFSET + SPEED_BYTES], "little", signed=True)/1000.0
            self.speed_mph = self.speed*2.23694
            self.heading = int.from_bytes(data[HEADING_OFFSET:HEADING_OFFSET + HEADING_BYTES], "little", signed=True)/1e5
            self.speed_accuracy = int.from_bytes(data[SPEED_ACCURACY_OFFSET:SPEED_ACCURACY_OFFSET + SPEED_ACCURACY_BYTES], "little")/1e3
            self.heading_accuracy = int.from_bytes(data[HEADING_ACCURACY_OFFSET:HEADING_ACCURACY_OFFSET + HEADING_ACCURACY_BYTES], "little")/1e5
            self.pdop = int.from_bytes(data[PDOP_OFFSET:PDOP_OFFSET + PDOP_BYTES], "little")/100.0
            self.latlon_flags = data[LATLON_FLAGS_OFFSET]
            self.input_voltage = data[INPUT_VOLTAGE_OFFSET]/10.0
            self.gforce_x = int.from_bytes(data[GFORCE_X_OFFSET:GFORCE_X_OFFSET + GFORCE_X_BYTES], "little", signed=True)/1e3
            self.gforce_y = int.from_bytes(data[GFORCE_Y_OFFSET:GFORCE_Y_OFFSET + GFORCE_Y_BYTES], "little", signed=True)/1e3
            self.gforce_z = int.from_bytes(data[GFORCE_Z_OFFSET:GFORCE_Z_OFFSET + GFORCE_Z_BYTES], "little", signed=True)/1e3
            self.rotation_rate_x = int.from_bytes(data[ROTATION_RATE_X_OFFSET:ROTATION_RATE_X_OFFSET + ROTATION_RATE_X_BYTES], "little", signed=True)/100
            self.rotation_rate_y = int.from_bytes(data[ROTATION_RATE_Y_OFFSET:ROTATION_RATE_Y_OFFSET + ROTATION_RATE_Y_BYTES], "little", signed=True)/100
            self.rotation_rate_z = int.from_bytes(data[ROTATION_RATE_Z_OFFSET:ROTATION_RATE_Z_OFFSET + ROTATION_RATE_Z_BYTES], "little", signed=True)/100

            send_zmq_json(socket, self.as_dict())

        def handle_rx(_: BleakGATTCharacteristic, data: bytearray):
            if(not verify_checksum(data)):
                logger.warn("Checksum failed!")
                return
            # TODO handle other payload types
            if data[4] != 0x50 or data[5] != 0x00:
                # This would indicate something other than the standard RaceBox message with all fields enabled.
                raise NotImplementedError
            update_class_data(data)

        def stall():
            time.sleep(5)

        async with BleakClient(device, disconnected_callback=handle_disconnect) as client:
            logger.debug("Connected... I think?")
            await client.start_notify(UART_TX_CHAR_UUID, handle_rx)
            logger.info("Connected to RaceBox")

            loop = asyncio.get_running_loop()

            while True:
                # TODO I don't really understand asyncio, but without this the with block will terminate and close the connection.
                #      This seems pretty silly, so learn how asyncio works and do something more appropriate
                await loop.run_in_executor(None, stall)

if __name__ == "__main__":
    # This is the serial number for my device. Update it to yours if you want to test the connection this way
    sn = 3242701007
    LOG_DIR = "/home/pi/log/racebox"
    os.umask(0)
    os.makedirs(LOG_DIR, exist_ok=True)

    log_file_name_template = "{:04}.log"
    last_log_number = 0
    log_files = sorted(os.listdir(LOG_DIR))
    if len(log_files) > 0:
        last_log_file_name = log_files[-1]
        last_log_number = int(last_log_file_name.split(".")[0].split("_")[-1])
    log_number = last_log_number + 1

    log_file_name = os.path.join(LOG_DIR, log_file_name_template.format(log_number))
    logging.basicConfig(filename=log_file_name, format="%(asctime)s %(message)s", filemode='w')
    logger = logging.getLogger()
    logger.setLevel(logging.INFO)

    logger.info("Starting racebox.py")

    while True:
        try:
            asyncio.run(RaceBoxData(sn).read_racebox(logger))
        except asyncio.CancelledError:
            # task is cancelled on disconnect, so we ignore this error
            logger.error("Device disconnected; retrying...")
            pass
        except bleak.exc.BleakDBusError:
            logger.error("bleak.exc.BleakDBusError again; retrying...")
