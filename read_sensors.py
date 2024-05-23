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

import racebox

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
gforce_x = None
gforce_y = None
gforce_z = None
gps_utc_date = None
gps_utc_time = None

# The other fields are filtered on the Arduino. They should probably be filtered here, or maybe even in the display application.
WATER_PRESS_FILTER_SAMPLE_COUNT = 10
water_press_filter_samples = [0]*WATER_PRESS_FILTER_SAMPLE_COUNT
water_press_filter_current_sample = 0

last_log_time = int(time.time())
# I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
next_log_time = last_log_time + 1

context = zmq.Context()
cluster_socket = context.socket(zmq.PUSH)
racebox_socket = context.socket(zmq.PULL)
cluster_socket.connect("tcp://localhost:9961")
racebox_socket.bind("tcp://*:9962")

# the only other socket should be read-only, but since there are multiple it feels wrong to assume which one we'll be using
def send_zmqpp(message, socket=cluster_socket):
    if "inf" not in message:
        logger.debug(message)
        socket.send(message.encode())
    else:
        logger.debug("Value is inf; skipping: {}".format(message))

def time_string_from_racebox_data(racebox_data):
    gps_utc_date = f"{racebox_data['year']}{racebox_data['month']:02}{racebox_data['day']:02}"
    gps_utc_time = f"{racebox_data['hour']:02}{racebox_data['minute']:02}{racebox_data['second']:02}"
    return f"{gps_utc_date}-{gps_utc_time}"


ttyUSBfiles = []
ttyUSBs = []
# We know the Arduino will be set to 921600, but the GPS may be 9600 or 57600 depending on whether it has been initialized already, so look for the Arduino.
# There are a lot of instances of try/except:pass here; if there's an error, keep going and hope it gets fixed because the driver isn't going to pull over to debug.
serial_device_dir = "/dev"
filenames = os.listdir(serial_device_dir)
for filename in filenames:
    if "ttyUSB" in filename:
        ttyUSBfiles.append(os.path.join(serial_device_dir, filename))
for ttyUSBfile in ttyUSBfiles:
    try:
        ttyUSBs.append(serial.Serial(ttyUSBfile, baudrate=921600, timeout=0.5))
    except serial.serialutil.SerialException as e:
        logger.error(e)

arduino = None

logger.debug("Attempting to determine which serial device is which")
if len(ttyUSBs) == 1:
    arduino = ttyUSB
# if there are no ttyUSB devices, this block will look for one forever
elif len(ttyUSBs) > 0:
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
# TODO is this still needed, or was it needed by read_gps?
time.sleep(0.1)

loops = 0

# Interval between log entries, in seconds
# TODO using this sort of logging method will not indicate stale data. Use something better.
LOG_INTERVAL = 0.1

gps_time = None

first_attempt = time.time()
while gps_time is None and time.time() - first_attempt < 5:
    try:
        racebox_data = racebox_socket.recv_json(flags=zmq.NOBLOCK)
        if racebox_data["date_time_flags"] & racebox.DATE_TIME_CONFIRM_AVAIL_MASK\
                and racebox_data["date_time_flags"] & racebox.DATE_VALID_MASK\
                and racebox_data["date_time_flags"] & racebox.TIME_VALID_MASK:
            gps_time = time_string_from_racebox_data(racebox_data)

            logger.info("Read UTC time from GPS ({}). Correcting log filename.".format(gps_time))
            # logging seems to play nicely with this, so it will just continue to log to the new file
            os.rename(log_file_name, os.path.join(LOG_DIR, "read_sensors_{}.log".format(gps_time)))
    except zmq.ZMQError:
        # we'll just continue to use the system time for this file and hope there's not a filename collision
        pass

# I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
next_log_time = int(time.time()) + 1

output_filename = os.path.join(TELEMETRY_DIR, "{}.csv".format(gps_time))
logger.info("Starting CSV output to {}".format(output_filename))
with open(output_filename, 'w', newline='') as csvfile:
    fieldnames = [
        "system_time",
        "nominal_time",
        "loop_time",
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
        "gforce_x",
        "gforce_y",
        "gforce_z",
        "repeated",
    ]
    csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    csv_writer.writeheader()

    while True:
        try:
            while True:
                try:
                    racebox_data = racebox_socket.recv_json(flags=zmq.NOBLOCK)
                    if "speed_mph" in racebox_data:
                        send_zmqpp("MPH:{}".format(racebox_data["speed_mph"]))
                    if "heading" in racebox_data:
                        track = racebox_data["heading"]
                    if "latitude" in racebox_data:
                        latitude = racebox_data["latitude"]
                    if "longitude" in racebox_data:
                        longitude = racebox_data["longitude"]
                    gps_utc_date = f"{racebox_data['year']}-{racebox_data['month']:02}-{racebox_data['day']:02}"
                    seconds = racebox_data["second"] + racebox_data["nanoseconds"]/1e9
                    gps_utc_time = f"{racebox_data['hour']:02}:{racebox_data['minute']:02}:{seconds:05.2}"
                    gforce_x = racebox_data["gforce_x"]
                    gforce_y = racebox_data["gforce_y"]
                    gforce_z = racebox_data["gforce_z"]
                    volts = racebox_data["input_voltage"]
                    last_raebox_update = time.time()
                except zmq.ZMQError:
                    break

            try:
                while arduino.in_waiting > 0:
                    try:
                        # TODO make this faster. May be an Arduino limitation, but it's slowing down the whole loop considerably (0.05-0.1s)
                        # splitting it off into yet another script and using more zmq communication may be the answer to accomplish this without moving permanently to async hell
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
                        # TODO remove this as the RaceBox logs volts. Leaving it in here as a reminder to take it out of the Arduino code.
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
                    "nominal_time": next_log_time,
                    "loop_time": current_time - last_log_time,
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
                    "gforce_x": gforce_x,
                    "gforce_y": gforce_y,
                    "gforce_z": gforce_z,
                    "repeated": 0,
                }
                csv_writer.writerow(fields)
                next_log_time += LOG_INTERVAL
                last_log_time = current_time
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

