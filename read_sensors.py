#!/usr/bin/python

import csv
import datetime
import os
import serial
import sys
import time
import zmq

import read_gps

KMH_TO_MPH = 0.621371

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

start = time.time()
last_log_time = time.time()

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://localhost:9961")

ttyUSBfiles = []
ttyUSBs = []
# We know the Arduino will be set to 921600, but the GPS may be 9600 or 57600 depending on whether it has been initialized already, so look for the Arduino.
# There are a lot of instances of try/except:pass here; if there's an error, keep going and hope it gets fixed because the driver isn't going to pull over to debug.
serial_device_dir = "/dev"
filenames = os.listdir(serial_device_dir)
for filename in filenames:
    if "ttyUSB" in filename:
        ttyUSBfiles.append(os.path.join(serial_device_dir, filename))
if len(ttyUSBfiles) != 2:
    # I genuinely don't know how to recover from this, so just stop
    socket.send("STOP\n".encode())
    sys.exit("Wrong number of USB serial devices: {}".format(len(ttyUSBfiles)))
for ttyUSBfile in ttyUSBfiles:
    try:
        ttyUSBs.append(serial.Serial(ttyUSBfile, baudrate=921600, timeout=0.5))
    except serial.serialutil.SerialException as e:
        print(e)

arduino = None
gps_port = None

print("Attempting to determine which serial device is which")
while arduino is None:
    # Use read() instead of readline() as the two devices will not use the same baudrate and \n will never be found using the wrong baudrate.
    for ttyUSB in ttyUSBs:
        try:
            print("Reading serial response")
            data = ttyUSB.read(1024).decode("utf-8")
            print("{}: {}".format(ttyUSB.name, data))
            if data is not None and "FUEL:" in data:
                print("Found FUEL: in {}".format(ttyUSB.name))
                arduino = ttyUSB
        except AttributeError as e:
            print(e)
for ttyUSB in ttyUSBs:
    if (ttyUSB != arduino):
        # let the GPS library handle the serial communication
        gps_port = ttyUSB.name
time.sleep(0.1)

try:
    read_gps.debug_enable()
    read_gps.setup_gps(gps_port)
except:
    pass

loops = 0

# Interval between log entries, in seconds
# TODO using this sort of logging method will not indicate stale data. Use something better.
LOG_INTERVAL = 0.1

output_filename = "/home/pi/car_log/{}.csv".format(datetime.datetime.now().strftime("%Y-%m-%dT%H.%M.%S"))
with open(output_filename, 'w', newline='') as csvfile:
    fieldnames = [
        "system_time",
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
    ]
    csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
    csv_writer.writeheader()

    while True:
        try:

            if time.time() - last_log_time >= LOG_INTERVAL:
                fields = {
                    "system_time": time.time(),
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
                    "track": track
                }
                csv_writer.writerow(fields)

            try:
                gps_data = read_gps.get_position_data(blocking=False)
                if "speed_kmh" in gps_data:
                    mph = gps_data["speed_kmh"] * KMH_TO_MPH
                    socket.send("MPH:{}".format(mph).encode())
                elif "track" in gps_data:
                    track = gps_data["track"]
                elif "latitude" in gps_data:
                    latitude = gps_data["latitude"]
                elif "longitude" in gps_data:
                    longitude = gps_data["longitude"]
                elif "date" in gps_data:
                    gps_utc_date = gps_data["date"]
                elif "utc" in gps_data:
                    gps_utc_time = gps_data["time"]
            except AttributeError:
                pass

            try:
                while arduino.in_waiting > 0:
                    try:
                        message = arduino.readline().decode("utf-8")
                        if "STOP" in message:
                            socket.send("STOP".encode())
                            sys.exit("STOP command received.")
                        (name, value) = message.split(":")
                        if name == "OT":
                            oil_temp = float(value)
                            socket.send("OT:{}".format(oil_temp).encode())
                        elif name == "OP":
                            oil_press = float(value)
                            socket.send("OP:{}".format(oil_press).encode())
                        elif name == "WT":
                            water_temp = float(value)
                            socket.send("WT:{}".format(water_temp).encode())
                        elif name == "WP":
                            water_press = float(value)
                            socket.send("WP:{}".format(water_press).encode())
                        elif name == "RPM":
                            rpm = int(value)
                            socket.send("RPM:{}".format(rpm).encode())
                        elif name == "FUEL":
                            fuel = float(value)
                            socket.send("FUEL:{}".format(fuel).encode())
                        elif name == "VOLTS":
                            fuel = float(value)
                            socket.send("VOLTS:{}".format(volts).encode())
                    except ValueError:
                        print(message)
            except AttributeError:
                pass

            time.sleep(0.01)
        except KeyboardInterrupt:
            break
        loops += 1

print("DONE")
print("{} loops.".format(loops))

