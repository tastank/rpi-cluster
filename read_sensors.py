#!/usr/bin/python

import csv
import datetime
import logging
import os
import serial
import sys
import time
import traceback
import zmq

import read_gps

LOG_DIR = "/home/pi/log/"
TELEMETRY_DIR = "/home/pi/car_log/"

log_file_name = os.path.join(LOG_DIR, "read_sensors_{}.log".format(int(time.time())))
logging.basicConfig(filename=log_file_name, format="%(asctime)s %(message)s", filemode='w')
logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

KNOTS_TO_MPH = 1.15078

rpm = None
oil_temp = None
oil_press = None
water_temp = None
water_press = None
fuel = None
mph = None
latitude = None
longitude = None
altitude = None
volts = None
track = None
gps_utc_date = None
gps_utc_time = None

# The other fields are filtered on the Arduino. They should probably be filtered here, or maybe even in the display application.
WATER_PRESS_FILTER_SAMPLE_COUNT = 10
water_press_filter_samples = [0]*WATER_PRESS_FILTER_SAMPLE_COUNT
water_press_filter_current_sample = 0

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://localhost:9961")

def send_zmqpp(message):
    if "inf" not in message:
        logger.debug(message)
        socket.send(message.encode())
    else:
        logger.debug("Value is inf; skipping: {}".format(message))

ttyUSBfiles = []
ttyUSBs = []
# We know the Arduino will be set to 921600, but the GPS may be 9600 or 57600 depending on whether it has been initialized already, so look for the Arduino.
# There are a lot of instances of try/except:pass here; if there's an error, keep going and hope it gets fixed because the driver isn't going to pull over to debug.
serial_device_dir = "/dev"
filenames = os.listdir(serial_device_dir)
for filename in filenames:
    if "ttyUSB" in filename:
        ttyUSBfiles.append(os.path.join(serial_device_dir, filename))
# if it's only 1, this will still function without GPS
if len(ttyUSBfiles) > 2:
    # I genuinely don't know how to recover from this, so just stop
    send_zmqpp("STOP\n".encode())
    sys.exit("Wrong number of USB serial devices: {}".format(len(ttyUSBfiles)))
for ttyUSBfile in ttyUSBfiles:
    try:
        ttyUSBs.append(serial.Serial(ttyUSBfile, baudrate=921600, timeout=0.5))
    except serial.serialutil.SerialException as e:
        logger.error(e)

arduino = None
gps_port = None

logger.debug("Attempting to determine which serial device is which")
while arduino is None:
    # Use read() instead of readline() as the two devices will not use the same baudrate and \n will never be found using the wrong baudrate.
    for ttyUSB in ttyUSBs:
        try:
            logger.debug("Reading serial response")
            data = ttyUSB.read(1024).decode("utf-8")
            logger.debug("{}: {}".format(ttyUSB.name, data))
            if data is not None and "FUEL:" in data:
                logger.debug("Found FUEL: in {}".format(ttyUSB.name))
                arduino = ttyUSB
        except AttributeError as e:
            logger.error(e)
for ttyUSB in ttyUSBs:
    if (ttyUSB != arduino):
        # let the GPS library handle the serial communication
        gps_port = ttyUSB.name
time.sleep(0.1)

try:
    read_gps.debug_enable()
    read_gps.setup_gps(gps_port)
except Exception as e:
    print(traceback.format_exc())

loops = 0

# Interval between log entries, in seconds
# TODO using this sort of logging method will not indicate stale data. Use something better.
LOG_INTERVAL = 0.1

gps_time = read_gps.get_gps_time()

logger.info("Read UTC time from GPS ({}). Correcting log filename.".format(gps_time))
# logging seems to play nicely with this, so it will just continue to log to the new file
os.rename(log_file_name, os.path.join(LOG_DIR, "read_sensors_{}.log".format(gps_time)))

# I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
next_log_time = int(time.time()) + 1

output_filename = os.path.join(TELEMETRY_DIR, "{}.csv".format(gps_time))
logger.info("Starting CSV output to {}".format(output_filename))
with open(output_filename, 'w', newline='') as csvfile:
    fieldnames = [
        "system_time",
        "nominal_time",
        "rpm",
        "oil_press",
        "oil_temp",
        "water_press",
        "water_temp",
        "volts",
        "fuel",
        "gps_utc_date",
        "gps_utc_time",
        "lat",
        "lon",
        "alt",
        "mph",
        "track",
        "repeated",
    ]
    csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    csv_writer.writeheader()

    while True:
        try:
            try:
                while (gps_data := read_gps.get_position_data(blocking=False)):
                    logger.debug("Read GPS data ({} message)".format(gps_data["type"]))
                    if "speed_kn" in gps_data:
                        mph = gps_data["speed_kn"] * KNOTS_TO_MPH
                        send_zmqpp("MPH:{}".format(mph))
                    if "track" in gps_data:
                        track = gps_data["track"]
                    if "latitude" in gps_data:
                        latitude = gps_data["latitude"]
                    if "longitude" in gps_data:
                        longitude = gps_data["longitude"]
                    if "date" in gps_data:
                        gps_utc_date = gps_data["date"]
                    if "utc" in gps_data:
                        gps_utc_time = gps_data["utc"]
            except AttributeError:
                pass

            try:
                while arduino.in_waiting > 0:
                    try:
                        # TODO make this faster. May be an Arduino limitation, but it's slowing down the whole loop considerably (0.05-0.1s)
                        message = arduino.readline().decode("utf-8")
                        # messages shouldn't be more than 100 chars. Something seems to be breaking but it's not leaving any error messages.
                        if len(message) > 100:
                            break
                        if "STOP" in message:
                            send_zmqpp("STOP")
                            sys.exit("STOP command received.")
                        (name, value) = message.split(":")
                        if name == "OT":
                            oil_temp = float(value)
                            send_zmqpp("OT:{}".format(oil_temp))
                        elif name == "OP":
                            oil_press = float(value)
                            send_zmqpp("OP:{}".format(oil_press))
                        elif name == "WT":
                            water_temp = float(value)
                            send_zmqpp("WT:{}".format(water_temp))
                        elif name == "WP":
                            water_press = float(value)
                            water_press_filter_current_sample = (water_press_filter_current_sample + 1) % WATER_PRESS_FILTER_SAMPLE_COUNT
                            water_press_filter_samples[water_press_filter_current_sample] = water_press
                            water_press_filtered = sum(water_press_filter_samples) / WATER_PRESS_FILTER_SAMPLE_COUNT
                            send_zmqpp("WP:{}".format(water_press_filtered))
                        elif name == "RPM":
                            rpm = int(value)
                            send_zmqpp("RPM:{}".format(rpm))
                        elif name == "FUEL":
                            fuel = float(value)
                            send_zmqpp("FUEL:{}".format(fuel))
                        elif name == "VOLTS":
                            volts = float(value)
                            send_zmqpp("VOLTS:{}".format(volts))
                        elif name == "FLASH":
                            send_zmqpp("FLASH:{}".format(float(value)))
                        else:
                            logger.info(message)
                    except ValueError:
                        logger.error(message)
            except AttributeError:
                pass
            # use a consistent time for the following checks
            current_time = time.time()
            if current_time >= next_log_time:
                fields = {
                    "system_time": current_time,
                    "nominal_time": next_log_time
                    "rpm": rpm,
                    "oil_press": oil_press,
                    "oil_temp": oil_temp,
                    "water_press": water_press,
                    "water_temp": water_temp,
                    "volts": volts,
                    "fuel": fuel,
                    "gps_utc_date": gps_utc_date,
                    "gps_utc_time": gps_utc_time,
                    "lat": latitude,
                    "lon": longitude,
                    "alt": altitude,
                    "mph": mph,
                    "track": track,
                    "repeated": 0,
                }
                csv_writer.writerow(fields)
                next_log_time += LOG_INTERVAL
                # if more than one LOG_INTERVAL has elapsed, repeat the observation as MoTeC is expecting a constant log rate.
                while current_time >= next_log_time:
                    logger.warn("Loop took longer than LOG_INTERVAL; repeating measurements")
                    fields["nominal_time"] = next_log_time
                    fields["repeated"] = 1
                    csv_writer.writerow(fields)
                    next_log_time += LOG_INTERVAL


            time.sleep(0.01)
        except KeyboardInterrupt:
            break
        loops += 1

logger.debug("DONE")
logger.debug("{} loops.".format(loops))

