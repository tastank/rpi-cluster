#!/usr/bin/python

import configparser
import copy
import csv
import datetime
import gpiozero
import logging
import os
import serial
import sys
import time
import traceback
import zmq

import racebox
import telemetry_upload

start_time = time.time()

CONFIG_FILE = "/home/pi/rpi-cluster/web.conf"
LOG_DIR = "/home/pi/log/read_sensors/"
TELEMETRY_DIR = "/home/pi/log/telemetry/"
os.umask(0)
os.makedirs(LOG_DIR, exist_ok=True)
os.makedirs(TELEMETRY_DIR, exist_ok=True)

log_file_name_template = "{:04}.log"
last_log_number = 0
log_files = sorted(os.listdir(LOG_DIR))
if len(log_files) > 0:
    last_log_file_name = log_files[-1]
    last_log_number = int(last_log_file_name.split(".")[0])
log_number = last_log_number + 1

log_file_name = os.path.join(LOG_DIR, log_file_name_template.format(log_number))
logging.basicConfig(filename=log_file_name, format="%(asctime)s %(message)s", filemode='w')
logger = logging.getLogger()
logger.setLevel(logging.INFO)

config = configparser.ConfigParser()
config.read(CONFIG_FILE)

cpu = gpiozero.CPUTemperature()

rpm = 0
oil_temp = 0
oil_temp_filtered = 0
oil_press = 0
oil_press_filtered = 0
water_temp = 0
water_temp_filtered = 0
water_press = 0
water_press_filtered = 0
fuel = 0
fuel_qty_filtered = 0
arduino_temp = 0
mph = 0
latitude = 0
longitude = 0
altitude = 0
volts = 0
track = 0
gforce_x = 0
gforce_y = 0
gforce_z = 0
gps_utc_date = ""
gps_utc_time = ""
cumulative_lap_distance = 0
lap_number = 0
beacon = 0

# TODO don't hardcode this
# TODO a track with a finish line not oriented directly EW or NS will be more complicated
# these numbers are significantly off-track, to allow for GPS error and pit stops
ROAD_AMERICA_FINISH_LINE_LEFT_LONGITUDE = -87.989493
ROAD_AMERICA_FINISH_LINE_RIGHT_LONGITUDE = -87.990064
ROAD_AMERICA_FINISH_LINE_LATITUDE = 43.797913

WATER_PRESS_FILTER_SAMPLE_COUNT = 10
water_press_filter_samples = [0]*WATER_PRESS_FILTER_SAMPLE_COUNT
water_press_filter_current_sample = 0
OIL_PRESS_FILTER_SAMPLE_COUNT = 10
oil_press_filter_samples = [0]*OIL_PRESS_FILTER_SAMPLE_COUNT
oil_press_filter_current_sample = 0
WATER_TEMP_FILTER_SAMPLE_COUNT = 64
water_temp_filter_samples = [0]*WATER_TEMP_FILTER_SAMPLE_COUNT
water_temp_filter_current_sample = 0
OIL_TEMP_FILTER_SAMPLE_COUNT = 64
oil_temp_filter_samples = [0]*OIL_TEMP_FILTER_SAMPLE_COUNT
oil_temp_filter_current_sample = 0
FUEL_QTY_FILTER_SAMPLE_COUNT = 255
fuel_qty_filter_samples = [0]*FUEL_QTY_FILTER_SAMPLE_COUNT
fuel_qty_filter_current_sample = 0
# this is the square of G force, to avoid the sqrt
FUEL_SENSOR_G_FORCE_SQ_THRESHOLD = 0.02

last_log_time = int(time.time())
# I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
next_log_time = last_log_time + 1

context = zmq.Context()
cluster_socket = context.socket(zmq.PUSH)
racebox_socket = context.socket(zmq.PULL)
cluster_socket.connect("tcp://localhost:9961")
racebox_socket.bind("tcp://*:9962")

fieldnames = [
    "system_time",
    "nominal_time",
    # TODO there has to be a better name than "iteration_time"; this is the elapsed time since data was last logged, and should equal LOG_INTERVAL on average
    "iteration_time",
    "loop_time",
    "rpm",
    "oil_press",
    "oil_press_filtered",
    "oil_temp",
    "oil_temp_filtered",
    "water_press",
    "water_press_filtered",
    "water_temp",
    "water_temp_filtered",
    "arduino_temp",
    "volts",
    "fuel",
    "fuel_qty_filtered",
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
    "beacon",
    "lap_number",
    "cumulative_lap_distance",
    "rpi_cpu_temp",
    "repeated",
]

if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
    uploader_fieldnames = copy.copy(fieldnames)
    # ignore these fields to conserve bandwidth
    for ignore_field in ["nominal_time", "iteration_time", "loop_time", "oil_press", "oil_temp", "water_press", "water_temp", "gps_utc_date"]:
        uploader_fieldnames.remove(ignore_field)
    telemetry_uploader = telemetry_upload.TelemetryUploader(
            fieldnames=uploader_fieldnames,
            address=config["TELEMETRY_UPLOAD"]["address"],
            port=config["TELEMETRY_UPLOAD"]["port"],
    )

# the only other socket should be read-only, but since there are multiple it feels wrong to assume which one we'll be using
def send_zmqpp(message, socket=cluster_socket):
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
for ttyUSBfile in ttyUSBfiles:
    try:
        ttyUSBs.append(serial.Serial(ttyUSBfile, baudrate=921600, timeout=0.5))
    except serial.serialutil.SerialException as e:
        logger.error(traceback.format_exc())

arduino = None

logger.debug("Attempting to find Arduino")
if len(ttyUSBs) == 1:
    logger.info("Only one ttyUSB device found, assuming it is the Arduino.")
    arduino = ttyUSBs[0]
# if there are no ttyUSB devices, this block will look for one forever
elif len(ttyUSBs) > 0:
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
# TODO is this still needed, or was it needed by read_gps?
time.sleep(0.1)
if arduino is None:
    logger.warn("Arduino not found!")
else:
    logger.debug("Arduino found: {}".format(arduino.name))

# Interval between log entries, in seconds
# TODO using this sort of logging method will not indicate stale data. Use something better.
LOG_INTERVAL = 0.1

# I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
next_log_time = int(time.time()) + 1

output_filename = os.path.join(TELEMETRY_DIR, "{:04}.csv".format(log_number))
logger.info("Starting CSV output to {}".format(output_filename))

laptime_start = time.time()
last_racebox_update = time.time()
previous_latitude = 0

with open(output_filename, 'w', newline='') as csvfile:
    csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    csv_writer.writeheader()

    while True:
        loop_start_time = time.time()
        try:
            send_zmqpp("TIME:{}".format(int((loop_start_time - start_time)/60)))
            while True:
                try:
                    racebox_data = racebox_socket.recv_json(flags=zmq.NOBLOCK)
                    mph = racebox_data["speed_mph"]
                    send_zmqpp("MPH:{}".format(mph))
                    track = racebox_data["heading"]
                    latitude = racebox_data["latitude"]
                    longitude = racebox_data["longitude"]
                    altitude = racebox_data["msl_altitude_ft"]
                    gps_utc_date = f"{racebox_data['year']}-{racebox_data['month']:02}-{racebox_data['day']:02}"
                    seconds = racebox_data["second"] + racebox_data["nanoseconds"]/1e9
                    gps_utc_time = f"{racebox_data['hour']:02}:{racebox_data['minute']:02}:{seconds:05.2f}"
                    gforce_x = racebox_data["gforce_x"]
                    gforce_y = racebox_data["gforce_y"]
                    gforce_z = racebox_data["gforce_z"]
                    volts = racebox_data["input_voltage"]
                    send_zmqpp("VOLTS:{}".format(volts))
                    current_time = time.time()
                    cumulative_lap_distance += mph * (current_time - last_racebox_update) / 3600.0
                    last_racebox_update = time.time()
                    # TODO more hardcoding to remove; should maybe just save this for the analysis script
                    if longitude < ROAD_AMERICA_FINISH_LINE_LEFT_LONGITUDE and longitude > ROAD_AMERICA_FINISH_LINE_RIGHT_LONGITUDE and latitude < ROAD_AMERICA_FINISH_LINE_LATITUDE and previous_latitude > ROAD_AMERICA_FINISH_LINE_LATITUDE and time.time() - laptime_start > 30:
                        laptime_end = current_time
                        beacon = 1
                        laptime = laptime_end - laptime_start
                        lap_number += 1
                        laptime_start = laptime_end
                        cumulative_lap_distance = 0
                    else:
                        beacon = 0
                    previous_latitude = latitude
                except zmq.ZMQError:
                    # the most likely reason for this is just that there's no data to read.
                    break

            try:
                while arduino is not None and arduino.in_waiting > 0:
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
                        if "RESET" in message:
                            logger.warn("Arduino reset")
                        (name, value) = message.split(":")
                        # TODO this is logging unfiltered values; is it better to just log the filtered values?
                        if name == "OT":
                            oil_temp = float(value)
                            oil_temp_filter_current_sample = (oil_temp_filter_current_sample + 1) % OIL_TEMP_FILTER_SAMPLE_COUNT
                            oil_temp_filter_samples[oil_temp_filter_current_sample] = oil_temp
                            oil_temp_filtered = sum(oil_temp_filter_samples) / OIL_TEMP_FILTER_SAMPLE_COUNT
                            send_zmqpp("OT:{}".format(oil_temp_filtered))
                        elif name == "OP":
                            oil_press = float(value)
                            oil_press_filter_current_sample = (oil_press_filter_current_sample + 1) % OIL_PRESS_FILTER_SAMPLE_COUNT
                            oil_press_filter_samples[oil_press_filter_current_sample] = oil_press
                            oil_press_filtered = sum(oil_press_filter_samples) / OIL_PRESS_FILTER_SAMPLE_COUNT
                            send_zmqpp("OP:{}".format(oil_press_filtered))
                        elif name == "WT":
                            water_temp = float(value)
                            water_temp_filter_current_sample = (water_temp_filter_current_sample + 1) % WATER_TEMP_FILTER_SAMPLE_COUNT
                            water_temp_filter_samples[water_temp_filter_current_sample] = water_temp
                            water_temp_filtered = sum(water_temp_filter_samples) / WATER_TEMP_FILTER_SAMPLE_COUNT
                            send_zmqpp("WT:{}".format(water_temp_filtered))
                        elif name == "WP":
                            water_press = float(value)
                            water_press_filter_current_sample = (water_press_filter_current_sample + 1) % WATER_PRESS_FILTER_SAMPLE_COUNT
                            water_press_filter_samples[water_press_filter_current_sample] = water_press
                            water_press_filtered = sum(water_press_filter_samples) / WATER_PRESS_FILTER_SAMPLE_COUNT
                            send_zmqpp("WP:{}".format(water_press_filtered))
                        elif name == "RPM":
                            rpm = int(value)
                            send_zmqpp("RPM:{}".format(rpm))
                        elif name == "AT":
                            arduino_temp = float(value)
                        elif name == "FUEL":
                            fuel = float(value)
                            # fuel sloshes a lot. Ignore the reading any time the car is under significant acceleration.
                            # gforce_z should be roughly 1, so ignore it in the computation; checking that the car is not bouncing may also be a good idea though.
                            # TODO it would be a good idea to make sure the car has been below the threshold for some time rather than just getting there now.
                            if (gforce_x*gforce_x + gforce_y*gforce_y) < FUEL_SENSOR_G_FORCE_SQ_THRESHOLD:
                                fuel_qty_filter_current_sample = (fuel_qty_filter_current_sample + 1) % FUEL_QTY_FILTER_SAMPLE_COUNT
                                fuel_qty_filter_samples[fuel_qty_filter_current_sample] = fuel
                                fuel_qty_filtered = sum(fuel_qty_filter_samples) / FUEL_QTY_FILTER_SAMPLE_COUNT
                            send_zmqpp("FUEL:{}".format(fuel_qty_filtered))
                        # TODO remove this as the RaceBox logs volts. Leaving it in here as a reminder to take it out of the Arduino code.
                        #elif name == "VOLTS":
                        #    volts = float(value)
                        #    send_zmqpp("VOLTS:{}".format(volts))
                        elif name == "FLASH":
                            send_zmqpp("FLASH:{}".format(float(value)))
                        else:
                            logger.debug(message)
                    except ValueError:
                        logger.error(traceback.format_exc())
                    except UnicodeDecodeError:
                        logger.error(traceback.format_exc())
            except AttributeError:
                logger.error(traceback.format_exc())
            # use a consistent time for the following checks
            current_time = time.time()
            if current_time >= next_log_time:
                fields = {
                    "system_time": round(current_time, 3),
                    "nominal_time": round(next_log_time, 3),
                    "iteration_time": round(current_time - last_log_time, 4),
                    "loop_time": round(current_time - loop_start_time, 4),
                    "rpm": rpm,
                    "oil_press": oil_press,
                    "oil_press_filtered": round(oil_press_filtered, 1),
                    "oil_temp": oil_temp,
                    "oil_temp_filtered": round(oil_temp_filtered, 1),
                    "water_press": water_press,
                    "water_press_filtered": round(water_press_filtered, 1),
                    "water_temp": water_temp,
                    "water_temp_filtered": round(water_temp_filtered, 1),
                    "arduino_temp": arduino_temp,
                    "volts": volts,
                    "fuel": fuel,
                    "fuel_qty_filtered": round(fuel_qty_filtered, 1),
                    "gps_utc_date": gps_utc_date,
                    "gps_utc_time": gps_utc_time,
                    "lat": latitude,
                    "lon": longitude,
                    "alt": altitude,
                    "mph": mph,
                    "track": round(track, 1),
                    "gforce_x": round(gforce_x, 3),
                    "gforce_y": round(gforce_y, 3),
                    "gforce_z": round(gforce_z, 3),
                    "beacon": beacon,
                    "lap_number": lap_number,
                    "cumulative_lap_distance": round(cumulative_lap_distance, 2),
                    "rpi_cpu_temp": cpu.temperature,
                    "repeated": 0,
                }
                csv_writer.writerow(fields)
                if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
                    telemetry_uploader.send_update(fields)
                next_log_time += LOG_INTERVAL
                last_log_time = current_time
                # if more than one LOG_INTERVAL has elapsed, repeat the observation as MoTeC is expecting a constant log rate.
                while current_time >= next_log_time:
                    logger.warn("Loop took longer than LOG_INTERVAL; repeating measurements")
                    fields["nominal_time"] = next_log_time
                    fields["repeated"] = 1
                    # in case this was the start of a new lap, don't repeat that
                    fields["beacon"] = 0
                    csv_writer.writerow(fields)
                    if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
                        telemetry_uploader.send_update(fields)
                    next_log_time += LOG_INTERVAL

            time.sleep(0.01)
        except KeyboardInterrupt:
            break
        except:
            # Don't just give up if there's an error
            logger.error(traceback.format_exc())

logger.debug("DONE")

