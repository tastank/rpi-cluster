
import zmq

class TelemetryUploader:
    def __init__(self, address, port, fieldnames):
        self.address = address
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUSH)
        self.socket.connect(f"tcp://{address}:{port}")
        self.updates = []
        self.set_fieldnames(fieldnames)

    def set_fieldnames(self, fieldnames):
        self.fieldnames = fieldnames
        self.socket.send_json({"fieldnames": self.fieldnames})

    def should_send_update(self):
        if int(self.updates[-1]["beacon"]) == 1:
            return True
        return False

    def send_update(self, update):
        self.updates.append(update)
        if self.should_send_update():
            print("Sending update")
            # TODO make this work with update strings that contain ","
            update_csv = "\n".join([",".join([str(row[fieldname]) for fieldname in self.fieldnames]) for row in self.updates])
            self.socket.send_json({"update_csv": update_csv})
            self.updates = []
