
import zmq


class TelemetryUploader:
    def __init__(self, address, port, fieldnames, logger):
        self.address = address
        self.port = port
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUSH)
        self.socket.connect(f"tcp://{address}:{port}")
        self.update_csv = ""
        self.num_updates = 0
        self.last_update = {}
        self.set_fieldnames(fieldnames)
        self.logger = logger
        self.logger.info("Telemetry upload initialized")

    def set_fieldnames(self, fieldnames):
        self.fieldnames = fieldnames
        self.socket.send_json({"fieldnames": self.fieldnames})

    def should_send_update(self):
        if "beacon" in self.last_update and int(self.last_update["beacon"]) == 1:
            return True
        return False

    def enqueue_update(self, update):
        self.last_update = update
        self.update_csv += ",".join([str(update[fieldname]) for fieldname in self.fieldnames]) + "\n"
        self.num_updates += 1
        # to limit memory use, limit stored updates to five minutes
        if self.num_updates > 25*60*5:
            self.update_csv = self.update_csv[self.update_csv.find("\n")+1:]

        if self.should_send_update():
            # TODO make this work with update strings that contain ","
            self.socket.send_json({"update_csv": self.update_csv})
            self.update_csv = ""
            self.num_updates = 0

