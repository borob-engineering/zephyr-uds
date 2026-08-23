import time
from PyQt5.QtCore import QThread, pyqtSignal
from uds_engine import UDSEngine

class UDSWorker(QThread):
    """Führt UDS-Befehle asynchron aus, um das Einfrieren der GUI zu verhindern."""
    response_received = pyqtSignal(str, str)  # Richtung (TX/RX/SYS), Payload/Nachricht
    error_occurred = pyqtSignal(str)

    def __init__(self, engine: UDSEngine, task_type: str, params: dict):
        super().__init__()
        self.engine = engine
        self.task_type = task_type
        self.params = params

    def run(self):
        try:
            if self.task_type == "connect":
                self.engine.connect()
                self.response_received.emit("SYS", "Erfolgreich mit ISO-TP Socket verbunden.")
            
            elif self.task_type == "session":
                sess_id = self.params.get("session_id", 0x01)
                self.response_received.emit("TX", f"10 {sess_id:02X}")
                resp = self.engine.change_session(sess_id)
                self.response_received.emit("RX", resp.hex(' ').upper() if resp else "KEINE ANTWORT")

            elif self.task_type == "read_did":
                did = self.params.get("did", 0x0000)
                self.response_received.emit("TX", f"22 {did:04X}")
                resp = self.engine.read_did(did)
                self.response_received.emit("RX", resp.hex(' ').upper() if resp else "KEINE ANTWORT")

            elif self.task_type == "write_did":
                did = self.params.get("did", 0x0000)
                data = self.params.get("data", [])
                data_str = " ".join(f"{x:02X}" for x in data)
                self.response_received.emit("TX", f"2E {did:04X} {data_str}")
                resp = self.engine.write_did(did, data)
                self.response_received.emit("RX", resp.hex(' ').upper() if resp else "KEINE ANTWORT")
                
        except Exception as e:
            self.error_occurred.emit(str(e))