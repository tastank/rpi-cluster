#!/usr/bin/python

import asyncio
import os
import serial
import sys
import traceback

class ArduinoSensors:
    def __init__(self, logger, arduino_serial=None, tty_filename=None):
        self.logger = logger
        self.arduino_serial = arduino_serial
        if self.arduino_serial is None:
            self.set_arduino_serial_device(tty_filename)
        # TODO initialize to None to indicate stale data?
        self.rpm = None
        self.oil_temp = None
        self.oil_press = None
        self.water_temp = None
        self.water_press = None
        self.fuel = None
        self.arduino_temp = None
        self.flash = None
        self.new_data_available = False
        self.dict = {
            "rpm": self.rpm,
            "oil_temp": self.oil_temp,
            "oil_press": self.oil_press,
            "water_temp": self.water_temp,
            "water_press": self.water_press,
            "fuel": self.fuel,
            "arduino_temp": self.arduino_temp,
            "flash": self.flash,
        }

    def set_arduino_serial_device(self, tty_filename=None):
        self.arduino_serial = None
        if tty_filename is None:
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
                    self.logger.error(traceback.format_exc())

            self.logger.debug("Attempting to find Arduino")
            if len(ttyUSBs) == 1:
                self.logger.info("Only one ttyUSB device found, assuming it is the Arduino.")
                self.arduino_serial = ttyUSBs[0]
            # if there are no ttyUSB devices, this block will look for one forever
            elif len(ttyUSBs) > 0:
                self.logger.debug("Attempting to determine which serial device is which")
                while self.arduino_serial is None:
                    # Use read() instead of readline() as the two devices will not use the same baudrate and \n will never be found using the wrong baudrate.
                    for ttyUSB in ttyUSBs:
                        try:
                            self.logger.debug("Reading serial response")
                            data = ttyUSB.read(1024).decode("utf-8")
                            self.logger.debug("{}: {}".format(ttyUSB.name, data))
                            if data is not None and "FUEL:" in data:
                                self.logger.debug("Found FUEL: in {}".format(ttyUSB.name))
                                self.arduino_serial = ttyUSB
                        except AttributeError as e:
                            self.logger.error(e)
        else:
            self.arduino_serial = serial.Serial(ttyUSBfile, baudrate=921600, timeout=0.5)

        if self.arduino_serial is None:
            self.logger.warn("Arduino not found!")
        else:
            self.logger.debug("Arduino found: {}".format(self.arduino_serial.name))

    def as_dict(self):
        return self.dict

    def reset(self):
        for key in self.dict:
            self.dict[key] = None
        self.new_data_available = False

    async def read_arduino_data(self):
        while True:
            try:
                while self.arduino_serial is not None and self.arduino_serial.in_waiting > 0:
                    try:
                        message = self.arduino_serial.readline().decode("utf-8")
                        # messages shouldn't be more than 100 chars. Something seems to be breaking but it's not leaving any error messages.
                        if len(message) > 100:
                            break
                        if "STOP" in message:
                            send_zmqpp("STOP")
                            sys.exit("STOP command received.")
                        if "RESET" in message:
                            self.logger.warn("Arduino reset")
                        (name, value) = message.split(":")
                        # TODO this is logging unfiltered values; is it better to just log the filtered values?
                        if name == "OT":
                            self.oil_temp = float(value)
                            self.dict["oil_temp"] = self.oil_temp
                        elif name == "OP":
                            self.oil_press = float(value)
                            self.dict["oil_press"] = self.oil_press
                        elif name == "WT":
                            self.water_temp = float(value)
                            self.dict["water_temp"] = self.water_temp
                        elif name == "WP":
                            self.water_press = float(value)
                            self.dict["water_press"] = self.water_press
                        elif name == "RPM":
                            self.rpm = int(value)
                            self.dict["rpm"] = self.rpm
                        elif name == "AT":
                            self.arduino_temp = float(value)
                            self.dict["arduino_temp"] = self.arduino_temp
                        elif name == "FUEL":
                            self.fuel = float(value)
                            self.dict["fuel"] = self.fuel
                        # TODO remove this as the RaceBox logs volts. Leaving it in here as a reminder to take it out of the Arduino code.
                        #elif name == "VOLTS":
                        #    volts = float(value)
                        #    send_zmqpp("VOLTS:{}".format(volts))
                        elif name == "FLASH":
                            self.flash = float(value)
                            self.dict["flash"] = self.flash
                        else:
                            self.logger.debug(message)
                        self.new_data_available = True
                    except ValueError:
                        self.logger.error(traceback.format_exc())
                    except UnicodeDecodeError:
                        self.logger.error(traceback.format_exc())
            except AttributeError:
                self.logger.error(traceback.format_exc())
            # TODO sleep for a more intelligent duration
            await asyncio.sleep(0.01)

