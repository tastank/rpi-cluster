import os
import posix
import time
import zmq

rpm_min = 0
rpm_max = 6700
rpm = rpm_min
rpm_inc = 8
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
mph_min = 0
mph_max = 120
mph = mph_min
mph_inc = 0.016
fuel_min = 0
fuel_max = 12
fuel = fuel_min
fuel_inc = 0.01
volts_min = 0
volts_max = 15
volts = volts_min
volts_inc = 0.01

start = time.time()

context = zmq.Context()
socket = context.socket(zmq.PUSH)
socket.connect("tcp://localhost:9961")

while time.time() - start < 60:
    try:
        socket.send("OP:{}".format(oil_press).encode())
        socket.send("OT:{}".format(oil_temp).encode())
        socket.send("WP:{}".format(water_press).encode())
        socket.send("WT:{}".format(water_temp).encode())
        socket.send("RPM:{}".format(rpm).encode())
        socket.send("MPH:{}".format(mph).encode())
        socket.send("FUEL:{}".format(fuel).encode())
        socket.send("VOLTS:{}".format(volts).encode())

        rpm += rpm_inc
        oil_temp += oil_temp_inc
        oil_press += oil_press_inc
        water_temp += water_temp_inc
        water_press += water_press_inc
        mph += mph_inc
        fuel += fuel_inc
        volts += volts_inc

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
        if mph >= mph_max or mph <= mph_min:
            mph_inc *= -1
        if fuel >= fuel_max or fuel <= fuel_min:
            fuel_inc *= -1
        if volts >= volts_max or volts <= volts_min:
            volts_inc *= -1

        time.sleep(0.01)
    except KeyboardInterrupt:
        break

print("DONE")

