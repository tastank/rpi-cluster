#!/usr/bin/python3

# Would it be better to use a proper GPS library? Perhaps. But I haven't found one that doesn't have a whole lot of baggage and isn't a pain to use, so I created this.
# Actually, I may have copy-pasted a lot of this from something else a long long time ago. I don't really remember writing it. If I did, I'm sorry.

import argparse
import csv
import os
import serial
import time
import traceback

_serial_gps = None

debug = 0

def debug_enable():
    global debug
    debug = 1

def debug_disable():
    global debug
    debug = 0

def debug_print(string, level=1):
    if debug >= level:
        print(string)

def debug_print_error(exception, level=0):
    if debug >= level:
        print("{}: {}".format(time.time(), traceback.format_exc()))

def nmea_to_iso_date(date):
    return "{}{}{}".format(date[4:6], date[2:4], date[0:2])

# In the NMEA message, the position gets transmitted as: 
# DDMM.MMMMM, where DD denotes the degrees and MM.MMMMM denotes
# the minutes. However, I want to convert this format to the following:
# DD.MMMM. This method converts a transmitted string to the desired format
def nmea_coords_to_degrees(nmea_coords):
    
    parts = nmea_coords.split(".")

    if (len(parts) != 2): 
        return nmea_coords

    degrees_digits = len(parts[0]) - 2
    
    left = parts[0]
    right = parts[1]
    degrees = float(left[:degrees_digits])
    minutes = float(left[degrees_digits:])
    minutes_fractional = float("0." + right)
    minutes += minutes_fractional

    degrees += minutes / 60.0

    return degrees

def get_gps_time():
    message_type = ""
    data = {}
    date_ = None
    time_ = None
    while date_ == None:
        try:
            data = get_position_data()
        except Exception as e:
            # there will be lots of errors here as the initial data will be incomplete; ignore them.
            pass
        if data and "type" in data and data["type"] == "$GPRMC" and data["status"] == "A":
            date_ = data["date"]
            time_ = data["utc"].split('.')[0]
    return "{}-{}".format(date_, time_)

# This method reads the data from the serial port, the GPS dongle is attached to, 
# and then parses the NMEA messages it transmits.
# gps is the serial port, that's used to communicate with the GPS adapter
def get_position_data(blocking=True):
    global _serial_gps
    if not blocking and _serial_gps.in_waiting == 0:
        return {}
    try:
        data = _serial_gps.readline().decode('utf-8')
    except UnicodeDecodeError:
        return
    debug_print(data, 2)

    message = data[0:6]
    parts = data.split(",")

    if (message == "$GPGGA"):
        raw_type, raw_utc, raw_lat, raw_ns, raw_lon, raw_ew, raw_quality, raw_num_sv, raw_hdop, raw_msl_alt, raw_alt_unit, raw_geoid_separation, raw_geoid_sep_unit, reference_station_id, raw_checksum = parts

        latitude = nmea_coords_to_degrees(raw_lat)
        longitude = nmea_coords_to_degrees(raw_lon)
        try:
            # TODO does this not lead to scope problems?
            msl_altitude = float(raw_msl_alt)
        except ValueError:
            # sometimes raw_msl_alt is "". Don't break if that happens.
            msl_altitude = None
        altitude_unit = raw_alt_unit.lower()
        try:
            geoid_separation = float(raw_geoid_separation)
        except ValueError:
            # See above re raw_msl_alt. I haven't seen this happen with raw_geoid_separation, but I wouldn't be surprised.
            geoid_separation = None
        geoid_sep_unit = raw_geoid_sep_unit.lower()
        quality = int(raw_quality)
        num_sv = int(raw_num_sv)

        # TODO make custom classes for this, maybe don't even do it in python
        return {
            "type": raw_type,
            "utc": raw_utc,
            "latitude": latitude,
            "ns": raw_ns,
            "longitude": longitude,
            "ew": raw_ew,
            "altitude": msl_altitude,
            "altitude_unit": altitude_unit,
            "geoid_separation": geoid_separation,
            "geoid_separation_unit": geoid_sep_unit,
            "quality": quality,
            "num_sv": num_sv,
        }
    elif message == "$GPRMC":
        raw_type, raw_utc, raw_status, raw_lat, raw_ns, raw_lon, raw_ew, raw_speed, raw_track, raw_date, raw_var, unknown_extra, raw_checksum = parts

        latitude = nmea_coords_to_degrees(raw_lat)
        longitude = nmea_coords_to_degrees(raw_lon)
        speed = float(raw_speed)
        track = float(raw_track),
        date = nmea_to_iso_date(raw_date)

        return {
            "type": raw_type,
            "utc": raw_utc,
            "latitude": latitude,
            "ns": raw_ns,
            "longitude": longitude,
            "ew": raw_ew,
            "speed_kmh": speed,
            "track": track,
            "date": date,
            "status": raw_status,
        }
    else:
        # TODO handle other NMEA messages and unsupported strings
        return {}

def setup_gps(serial_port):
    debug_print("Setting up serial GPS device on {}...".format(serial_port))
    global _serial_gps

    debug_print(" Initializing device...")
    ser = serial.Serial(serial_port, baudrate=9600, timeout=0.5)
    # TODO add support for setting the baud/sample rate to whatever you want and generating the command strings

    debug_print(" Setting GPS baudrate to 57600")
    ser.write(b"$PMTK251,57600*2C\r\n")
    ser.close()
    time.sleep(0.1)

    _serial_gps = serial.Serial(serial_port, baudrate = 57600, timeout = 0.5)
    debug_print(" Attempting to set sample rate...")
    gps_data = None
    # make sure the gps sample rate command succeeds. For some reason using _serial_gps.reset_output_buffer() immediately before this doesn't ensure there won't be other messages in the queue, so just keep sending the command until the success response is received.
    while gps_data != b"$PMTK001,220,3*30\r\n":
        _serial_gps.write(b"$PMTK220,200*2C\r\n")
        _serial_gps.write(b"$PMTK220,100*2F\r\n")
        gps_data = _serial_gps.readline()
        try:
            debug_print("  Response: {}".format(gps_data.decode("utf-8")))
        except UnicodeDecodeError as e:
            # There may be some corrupted data left from the baudrate change. Again, using reset_output_buffer() does not always clear that. The next read will succeed.
            debug_print(e)
    debug_print("Setup complete.")

if __name__ == "__main__":
    start = time.time()
    running = True
    parser = argparse.ArgumentParser(description="Log GPS data")
    parser.add_argument("-d", "--output-dir", help="Directory to log CSV output", default=".")
    parser.add_argument("-v", "--verbose", action='count', help="Verbose output", default=0)
    parser.add_argument("-s", "--serial-device", help="Path to tty device to which GPS module is attached", default="/dev/ttyUSB0")
    args = parser.parse_args()
    debug = args.verbose

    debug_print("Application started!")

    setup_gps(args.serial_port)

    current_data_cache = dict()
    timestamp = 0

    time_ = get_gps_time()
    output_filename = os.path.join(args.output_dir, "auto_log_{}.csv".format(time_))

    with open(output_filename, 'w', newline='') as csvfile:
        fieldnames = [
            "system_time",
            "utc_date",
            "utc_time",
            "lat",
            "ns",
            "lon",
            "ew",
            "alt",
            "alt_unit",
            "speed",
            "track",
            "quality",
            "num_sv",
            "status",
        ]
        csv_writer = csv.DictWriter(csvfile, fieldnames=fieldnames)
        csv_writer.writeheader()
        while running:
            try:
                data = get_position_data()
                if data:
                    if abs(float(data["utc"]) - timestamp) > 0.02:
                        debug_print(current_data_cache, 2)
                        if current_data_cache and "$GPGGA" in current_data_cache and "$GPRMC" in current_data_cache:
                            fields = {
                                "system_time": time.time(),
                                "utc_date": current_data_cache["$GPRMC"]["date"],
                                "utc_time": current_data_cache["$GPRMC"]["utc"],
                                "lat": current_data_cache["$GPGGA"]["latitude"],
                                "ns": current_data_cache["$GPGGA"]["ns"],
                                "lon": current_data_cache["$GPGGA"]["longitude"],
                                "ew": current_data_cache["$GPGGA"]["ew"],
                                "alt": current_data_cache["$GPGGA"]["altitude"],
                                "alt_unit": current_data_cache["$GPGGA"]["altitude_unit"],
                                "speed": current_data_cache["$GPRMC"]["speed"],
                                "track": current_data_cache["$GPRMC"]["track"],
                                "quality": current_data_cache["$GPGGA"]["quality"],
                                "num_sv": current_data_cache["$GPGGA"]["num_sv"],
                                "status": current_data_cache["$GPRMC"]["status"],
                            }
                            debug_print("utc = {utc} lat = {lat:.5f}{ns} lon = {lon:.5f}{ew} alt={alt:.1f}{alt_unit} quality={qual} number sv={num_sv}".format(
                                utc=fields["utc_time"],
                                lat=fields["lat"],
                                ns=fields["ns"],
                                lon=fields["lon"],
                                ew=fields["ew"],
                                alt=fields["alt"],
                                alt_unit=fields["alt_unit"],
                                qual=fields["quality"],
                                num_sv=fields["num_sv"],
                            ))

                            csv_writer.writerow(fields)
                        current_data_cache = dict()
                        timestamp = float(data["utc"])
                    current_data_cache[data["type"]] = data

            except KeyboardInterrupt:
                running = False
                _serial_gps.close()
                debug_print("Application closed!")
            except Exception as e:
                debug_print_error(e)
