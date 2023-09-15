import serial
import time
import zmq

import read_gps

KMH_TO_MPH = 0.621371

rpm_min = 0
rpm_max = 6700
rpm = rpm_min
rpm_inc = 2
oil_temp_min = 0
oil_temp_max = 270
oil_temp = oil_temp_min
oil_temp_inc = .1
oil_press_min = 0
oil_press_max = 90
oil_press = oil_press_min
oil_press_inc = .05
water_temp_min = 0
water_temp_max = 240
water_temp = water_temp_min
water_temp_inc = .06
water_press_min = 0
water_press_max = 20
water_press = water_press_min
water_press_inc = .02
fuel_min = 0
fuel_max = 12
fuel = fuel_min
fuel_inc = 0.01
mph = None
latitude = None
longitude = None

start = time.time()

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://localhost:9961")

ttyUSB0 = serial.Serial("/dev/ttyUSB0", baudrate=921600, timeout=0.5)
ttyUSB1 = serial.Serial("/dev/ttyUSB1", baudrate=921600, timeout=0.5)

arduino_port = None
gps_port = None

print("Attempting to determine which serial device is which")
while arduino_port is None:
    data_0 = None
    data_1 = None
    print("Reading serial response")
    # Use read() instead of readline() as the two devices will not use the same baudrate and \n will never be found using the wrong baudrate.
    # We know the Arduino will be set to 921600, but the GPS may be 9600 or 57600 depending on whether it has been initialized already, so look for the Arduino.
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
    if data_0 is not None and "OT10" in data_0:
        print("Found OT10 in /dev/ttyUSB0")
        arduino_port = "/dev/ttyUSB0"
        gps_port = "/dev/ttyUSB1"
    elif data_1 is not None and "OT10" in data_1:
        print("Found OT10 in /dev/ttyUSB1")
        arduino_port = "/dev/ttyUSB1"
        gps_port = "/dev/ttyUSB0"
    time.sleep(0.1)

ttyUSB0.close()
ttyUSB1.close()

read_gps.debug_enable()
read_gps.setup_gps(gps_port)

loops = 0

while time.time() - start < 10:
    try:
        gps_data = read_gps.get_position_data(blocking=False)
        if "speed_kmh" in gps_data:
            mph = gps_data["speed_kmh"] * KMH_TO_MPH
            socket.send("MPH:{}".format(mph).encode())
        else:
            mph = None


        socket.send("OP:{}".format(oil_press).encode())
        socket.send("OT:{}".format(oil_temp).encode())
        socket.send("WP:{}".format(water_press).encode())
        socket.send("WT:{}".format(water_temp).encode())
        socket.send("RPM:{}".format(rpm).encode())
        socket.send("FUEL:{}".format(fuel).encode())

        rpm += rpm_inc
        oil_temp += oil_temp_inc
        oil_press += oil_press_inc
        water_temp += water_temp_inc
        water_press += water_press_inc
        fuel += fuel_inc

        if rpm >= rpm_max or rpm <= rpm_min:
            rpm_inc *= -1
        if oil_temp >= oil_temp_max or oil_temp <= oil_temp_min:
            oil_temp_inc *= -1
        if oil_press >= oil_press_max or oil_press <= oil_press_min:
            oil_press_inc *= -1
        if water_temp >= water_temp_max or water_temp <= water_temp_min:
            water_temp_inc *= -1
        if water_press >= water_press_max or water_press <= water_press_min:
            water_press_inc *= -1
        if fuel >= fuel_max or fuel <= fuel_min:
            fuel_inc *= -1

        time.sleep(0.01)
    except KeyboardInterrupt:
        break
    loops += 1

print("DONE")
print("{} loops.".format(loops))

