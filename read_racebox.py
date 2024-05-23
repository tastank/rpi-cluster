import time
import zmq

context = zmq.Context()
racebox_socket = context.socket(zmq.PULL)
racebox_socket.bind("tcp://*:9962")

print_interval_s = 1
next_print = time.time()

while True:
    try:
        racebox_data = racebox_socket.recv_json(flags=0)#zmq.NOBLOCK)
        ns = racebox_data["nanoseconds"] / 1e9
        seconds = racebox_data["second"] + ns
        if True:#time.time() > next_print:
            print(f"{racebox_data['year']}-{racebox_data['month']:02}-{racebox_data['day']:02} {racebox_data['hour']:02}:{racebox_data['minute']:02}:{seconds:05.2f} | {racebox_data['latitude']:.1f}, {racebox_data['longitude']:.1f} | {racebox_data['speed_mph']:02.1f}mph | gx:{racebox_data['gforce_x']:1.2f} gy:{racebox_data['gforce_y']:1.2f} gz:{racebox_data['gforce_z']:1.2f} | {racebox_data['heading']}")
            #next_print += print_interval_s
    except zmq.ZMQError:
        time.sleep(0.01)
    

