import psutil
import subprocess
import time
import zmq

context = zmq.Context()
socket = context.socket(zmq.PULL)
socket.connect("tcp://localhost:9961")

while True:
    try:
        cluster_found = False
        for process in psutil.process_iter():
            if "cluster" == process.name():
                cluster_found = True
                break
        print("cluster_found: {}".format(cluster_found))
        if not cluster_found:
            subprocess.run(["/home/pi/rpi-cluster/cluster"])
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
