import serial
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

start = time.time()

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://localhost:9961")

# We know the Arduino will be set to 921600, but the GPS may be 9600 or 57600 depending on whether it has been initialized already, so look for the Arduino.
ttyUSB0 = serial.Serial("/dev/ttyUSB0", baudrate=921600, timeout=0.5)
ttyUSB1 = serial.Serial("/dev/ttyUSB1", baudrate=921600, timeout=0.5)

arduino = None
gps_port = None

print("Attempting to determine which serial device is which")
while gps_port is None:
    data_0 = None
    data_1 = None
    print("Reading serial response")
    # Use read() instead of readline() as the two devices will not use the same baudrate and \n will never be found using the wrong baudrate.
    try:
        data_0 = ttyUSB0.read(1024).decode("utf-8")
        print("ttyUSB0: {}".format(data_0))
    except:
        pass
    try:
        data_1 = ttyUSB1.read(1024).decode("utf-8")
        print("ttyUSB1: {}".format(data_1))
    except:
        pass
    if data_0 is not None and "FUEL:" in data_0:
        print("Found FUEL: in /dev/ttyUSB0")
        # let the GPS library handle the serial communication
        gps_port = "/dev/ttyUSB1"
        arduino = ttyUSB0
        ttyUSB1.close()
    elif data_1 is not None and "FUEL:" in data_1:
        print("Found FUEL: in /dev/ttyUSB1")
        gps_port = "/dev/ttyUSB0"
        arduino = ttyUSB1
        ttyUSB0.close()
    time.sleep(0.1)


read_gps.debug_enable()
read_gps.setup_gps(gps_port)

loops = 0

while time.time() - start < 60:
    try:
        gps_data = read_gps.get_position_data(blocking=False)
        if "speed_kmh" in gps_data:
            mph = gps_data["speed_kmh"] * KMH_TO_MPH
            socket.send("MPH:{}".format(mph).encode())
        else:
            mph = None

        while arduino.in_waiting > 0:
            try:
                message = arduino.readline().decode("utf-8")
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
                    socket.send("VOLTS:{}".format(fuel).encode())
            except ValueError:
                print(message)

        time.sleep(0.01)
    except KeyboardInterrupt:
        break
    loops += 1

print("DONE")
print("{} loops.".format(loops))

