#!/usr/bin/python

import asyncio
import configparser
import copy
import csv
import datetime
import gpiozero
import logging
import os
import time
import traceback
import zmq

import arduino_sensors
import racebox
import telemetry_upload

RACEBOX_SN = 3242701007

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

# TODO don't hardcode this
# TODO a track with a finish line not oriented directly EW or NS will be more complicated
# these numbers are significantly off-track, to allow for GPS error and pit stops
ROAD_AMERICA_FINISH_LINE_LEFT_LONGITUDE = -87.989493
ROAD_AMERICA_FINISH_LINE_RIGHT_LONGITUDE = -87.990064
ROAD_AMERICA_FINISH_LINE_LATITUDE = 43.797913

filter_sample_counts = {
    "water_press": 64,
    "oil_press": 10,
    "water_temp": 64,
    "oil_temp": 64,
    "fuel": 255,
}
filter_samples = {key: [0]*filter_sample_counts[key] for key in filter_sample_counts}
filter_current_sample = {key: 0 for key in filter_sample_counts}
# this is the square of G force, to avoid the sqrt
FUEL_SENSOR_G_FORCE_SQ_THRESHOLD = 0.02

def rolling_average_filter_insert(key, value):
    global filter_sample_counts, filter_samples, filter_current_sample
    if key not in filter_sample_counts:
        raise KeyError(f"Attempting to filter a value for which no filter was defined: {key}")
    filter_samples[key][filter_current_sample[key]] = value
    filter_current_sample[key] = (filter_current_sample[key] + 1) % filter_sample_counts[key]

def rolling_average_filter_get(key):
    global filter_sample_counts, filter_samples, filter_current_sample
    return round(sum(filter_samples[key])/filter_sample_counts[key], 1)

context = zmq.Context()
cluster_socket = context.socket(zmq.PUSH)
cluster_socket.connect("tcp://localhost:9961")

# these fields come directly from data reported by the RaceBox or Arduino, respectively
racebox_fields = [
    "latitude",
    "longitude",
    "msl_altitude_ft",
    "speed_mph",
    "heading",
    "gforce_x",
    "gforce_y",
    "gforce_z",
    "input_voltage",
]
arduino_fields = [
    "rpm",
    "oil_temp",
    "oil_press",
    "water_temp",
    "water_press",
    "arduino_temp",
    "fuel",
]

# these need some processing
fieldnames = [
    "system_time",
    "nominal_time",
    # TODO there has to be a better name than "iteration_time"; this is the elapsed time since data was last logged, and should equal LOG_INTERVAL on average
    "iteration_time",
    "loop_time",
    "gps_utc_date",
    "gps_utc_time",
    "oil_press_filtered",
    "oil_temp_filtered",
    "water_press_filtered",
    "water_temp_filtered",
    "fuel_filtered",
    "beacon",
    "lap_number",
    "cumulative_lap_distance",
    "rpi_cpu_temp",
    "repeated",
]
fieldnames.extend(racebox_fields)
fieldnames.extend(arduino_fields)
fields = {fieldname: 0 for fieldname in fieldnames}

rounded_fields = {
    "system_time": 3,
    "nominal_time": 3,
    "iteration_time": 4,
    "loop_time": 4,
    "heading": 1,
    "gforce_x": 3,
    "gforce_y": 3,
    "gforce_z": 3,
    "cumulative_lap_distance": 4,
}

if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
    uploader_fieldnames = copy.copy(fieldnames)
    # ignore these fields to conserve bandwidth
    for ignore_field in ["nominal_time", "iteration_time", "loop_time", "oil_press", "oil_temp", "water_press", "water_temp", "gps_utc_date"]:
        uploader_fieldnames.remove(ignore_field)
    telemetry_uploader = telemetry_upload.TelemetryUploader(
            fieldnames=uploader_fieldnames,
            address=config["TELEMETRY_UPLOAD"]["address"],
            port=config["TELEMETRY_UPLOAD"]["port"],
            logger=logger,
    )

# the only other socket should be read-only, but since there are multiple it feels wrong to assume which one we'll be using
def send_zmqpp(message, socket=cluster_socket):
    if "inf" not in message:
        logger.debug(message)
        socket.send(message.encode())
    else:
        logger.debug("Value is inf; skipping: {}".format(message))

def send_zmq_data(key, value, socket=cluster_socket):
    send_zmqpp(f"{key}:{value}", socket)

# Interval between log entries, in seconds
# TODO using this sort of logging method will not indicate stale data. Use something better.
LOG_INTERVAL = 0.04

output_filename = os.path.join(TELEMETRY_DIR, "{:04}.csv".format(log_number))
logger.info("Starting CSV output to {}".format(output_filename))

previous_latitude = 0

ZMQ_NAME = {
    "fuel": "FUEL",
    "speed_mph": "MPH",
    "input_voltage": "VOLTS",
    # TODO what is the key for "TIME"?
    "stint_time": "TIME",
    "oil_temp": "OT",
    "oil_press": "OP",
    "water_temp": "WT",
    "water_press": "WP",
    "rpm": "RPM",
    "flash": "FLASH",
}


# TODO give this a more descriptive name
async def main_loop(racebox_data, arduino_data):
    # TODO pack these all into a single dict
    global fields, output_filename
    last_racebox_update = time.monotonic()
    laptime_start = time.monotonic()
    last_log_time = int(time.monotonic())
    # I'm OK with missing a second of data to not log a bunch of repeats to fill the gap between the floor of the timestamp and the actual start time
    next_log_time = last_log_time + 1

    cumulative_lap_distance = 0
    with open(output_filename, 'w', newline='') as csvfile:
        csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        csv_writer.writeheader()

        while True:
            loop_start_time = time.monotonic()
            try:
                if arduino_data.new_data_available:
                    arduino_data_dict = arduino_data.as_dict()
                    for key in arduino_data_dict:
                        if arduino_data_dict[key] is not None and key in fields:
                            filtered_key = key + "_filtered"
                            fields[key] = arduino_data_dict[key]
                            if key in filter_samples and (key != "fuel" or (fields["gforce_x"]**2 + fields["gforce_y"]**2) < FUEL_SENSOR_G_FORCE_SQ_THRESHOLD):
                                rolling_average_filter_insert(key, arduino_data_dict[key])
                                fields[filtered_key] = rolling_average_filter_get(key)
                                send_zmq_data(ZMQ_NAME[key], fields[filtered_key])
                            elif key not in filter_samples and key in ZMQ_NAME:
                                send_zmq_data(ZMQ_NAME[key], fields[key])

                    arduino_data.reset()

                send_zmq_data("TIME", int(time.clock_gettime(time.CLOCK_BOOTTIME)/60))
                if racebox_data.new_data_available:
                    racebox_data_dict = racebox_data.limited_as_dict()
                    for key in racebox_data_dict:
                        fields[key] = racebox_data_dict[key]
                        if key in ZMQ_NAME:
                            send_zmq_data(ZMQ_NAME[key], fields[key])
                    fields["gps_utc_date"] = f"{racebox_data.year}-{racebox_data.month:02}-{racebox_data.day:02}"
                    seconds = racebox_data.second + racebox_data.nanoseconds/1e9
                    fields["gps_utc_time"] = f"{racebox_data.hour:02}:{racebox_data.minute:02}:{seconds:05.2f}"
                    current_time = time.monotonic()
                    # don't write cumulative_lap_distance directly to the fields dict as it will be rounded
                    cumulative_lap_distance += fields["speed_mph"] * (current_time - last_racebox_update) / 3600.0
                    last_racebox_update = current_time
                    # TODO more hardcoding to remove; should maybe just save this for the analysis script
                    if fields["longitude"] < ROAD_AMERICA_FINISH_LINE_LEFT_LONGITUDE and fields["longitude"] > ROAD_AMERICA_FINISH_LINE_RIGHT_LONGITUDE and fields["latitude"] < ROAD_AMERICA_FINISH_LINE_LATITUDE and previous_latitude > ROAD_AMERICA_FINISH_LINE_LATITUDE and time.monotonic() - laptime_start > 30:
                        laptime_end = current_time
                        fields["beacon"] = 1
                        laptime = laptime_end - laptime_start
                        fields["lap_number"] += 1
                        laptime_start = laptime_end
                        cumulative_lap_distance = 0
                    else:
                        fields["beacon"] = 0
                    previous_latitude = fields["latitude"]
                    racebox_data.new_data_available = False

                # use a consistent time for the following checks
                current_time = time.monotonic()
                if current_time >= next_log_time:
                    fields["system_time"] = current_time
                    fields["nominal_time"] = next_log_time
                    fields["iteration_time"] = current_time - last_log_time
                    fields["loop_time"] = current_time - loop_start_time
                    fields["rpi_cpu_temp"] = cpu.temperature,
                    fields["repeated"] = 0,
                    for fieldname in rounded_fields:
                        fields[fieldname] = round(fields[fieldname], rounded_fields[fieldname])
                    csv_writer.writerow(fields)
                    if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
                        telemetry_uploader.enqueue_update(fields)
                    next_log_time += LOG_INTERVAL
                    last_log_time = current_time
                    # make sure the next observation does not indicate a new lap
                    fields["beacon"] = 0
                    # if more than one LOG_INTERVAL has elapsed, repeat the observation as MoTeC is expecting a constant log rate.
                    while current_time >= next_log_time:
                        logger.warn("Loop took longer than LOG_INTERVAL; repeating measurements")
                        fields["nominal_time"] = next_log_time
                        # do not repeat starting of a new lap
                        fields["beacon"] = 0
                        fields["repeated"] = 1
                        csv_writer.writerow(fields)
                        if config["TELEMETRY_UPLOAD"]["upload_enabled"] == "1":
                            telemetry_uploader.enqueue_update(fields)
                        next_log_time += LOG_INTERVAL

                await asyncio.sleep(0.01)
            except KeyboardInterrupt:
                break
            except:
                # Don't just give up if there's an error
                logger.error(traceback.format_exc())
                print(traceback.format_exc())

    logger.debug("DONE")

async def main(racebox_data, arduino_data):
    await asyncio.gather(main_loop(racebox_data, arduino_data), racebox_data.read_racebox(), arduino_data.read_arduino_data())

if __name__ == "__main__":
    racebox_data = racebox.RaceBoxData(RACEBOX_SN, logger, limited_fields=racebox_fields)
    arduino_data = arduino_sensors.ArduinoSensors(logger=logger)
    asyncio.run(main(racebox_data, arduino_data))
