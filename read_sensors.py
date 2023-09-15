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

# TODO figure out what port the GPS module is connected to and use that. It might not be USB0.
read_gps.setup_gps("/dev/ttyUSB0")

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

