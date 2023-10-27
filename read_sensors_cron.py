import psutil
import subprocess
import time
import zmq

context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.connect("tcp://localhost:9961")

while True:
    try:
        read_sensors_found = False
        for process in psutil.process_iter():
            # use >= in case arguments are added in the future
            if "python" in process.name() and len(process.cmdline()) >= 2 and "read_sensors.py" in process.cmdline()[1]:
                read_sensors_found = True
                break
        print("read_sensors_found: {}".format(read_sensors_found))
        if not read_sensors_found:
            subprocess.run(["python", "/home/pi/rpi-cluster/read_sensors.py"])
        message = ""
        try:
            while "STOP" not in message:
                print("Reading message")
                message = socket.recv(flags=zmq.NOBLOCK)
                print(message)
        except:
            print("No ZMQ messages found.")
            pass
        if "STOP" in message:
            sys.exit("STOP received.")
        time.sleep(5)
    except SystemExit:
        break
    except KeyboardInterrupt:
        break
    except:
        pass
