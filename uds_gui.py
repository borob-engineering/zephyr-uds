import sys
import time
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QPushButton, QTextEdit, QLabel, QComboBox, QLineEdit, QHBoxLayout)
from uds_engine import UDSEngine
from uds_worker import UDSWorker

class UDSHardwareTesterGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.engine = UDSEngine(interface='can0')
        self.worker = None
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("UDS ISO-TP Hardware Tester (Modular)")
        self.resize(700, 500)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        layout = QVBoxLayout(central_widget)

        # Connection
        self.btn_connect = QPushButton("1. Verbinde ISO-TP (can0)")
        self.btn_connect.clicked.connect(self.start_connect)
        layout.addWidget(self.btn_connect)
        
        # Session Control
        layout.addWidget(QLabel("<b>UDS Diagnostic Session (0x10):</b>"))
        session_layout = QHBoxLayout()
        self.combo_session = QComboBox()
        self.combo_session.addItems(["Default Session (0x01)", "Programming Session (0x02)", "Extended Session (0x03)"])
        self.btn_send_session = QPushButton("Session wechseln")
        self.btn_send_session.clicked.connect(self.start_session_change)
        session_layout.addWidget(self.combo_session)
        session_layout.addWidget(self.btn_send_session)
        layout.addLayout(session_layout)

        # DID Control
        layout.addWidget(QLabel("<b>Read/Write Data By Identifier (0x22 / 0x2E):</b>"))
        did_layout = QHBoxLayout()
        self.txt_did = QLineEdit("F1 90")
        self.txt_did.setPlaceholderText("DID (z.B. F190)")
        self.txt_did_data = QLineEdit("")
        self.txt_did_data.setPlaceholderText("Schreibdaten (Hex-Bytes z.B. 11 22 33)")
        self.btn_read_did = QPushButton("Lesen (0x22)")
        self.btn_read_did.clicked.connect(self.start_read_did)
        self.btn_write_did = QPushButton("Schreiben (0x2E)")
        self.btn_write_did.clicked.connect(self.start_write_did)
        
        did_layout.addWidget(self.txt_did)
        did_layout.addWidget(self.txt_did_data)
        did_layout.addWidget(self.btn_read_did)
        did_layout.addWidget(self.btn_write_did)
        layout.addLayout(did_layout)

        # Log Output
        layout.addWidget(QLabel("<b>Terminal Log:</b>"))
        self.log_view = QTextEdit()
        self.log_view.setReadOnly(True)
        layout.addWidget(self.log_view)

    def run_async_task(self, task_type: str, params: dict):
        if self.worker and self.worker.isRunning():
            self.log_output("SYS", "Ein anderer Befehl wird noch ausgeführt...")
            return
        self.worker = UDSWorker(self.engine, task_type, params)
        self.worker.response_received.connect(self.log_output)
        self.worker.error_occurred.connect(self.log_error)
        self.worker.start()

    def start_connect(self):
        self.run_async_task("connect", {})

    def start_session_change(self):
        mapping = [0x01, 0x02, 0x03]
        session_id = mapping[self.combo_session.currentIndex()]
        self.run_async_task("session", {"session_id": session_id})

    def parse_did(self) -> int:
        clean = self.txt_did.text().replace(" ", "")
        return int(clean, 16) if clean else 0x0000

    def start_read_did(self):
        try:
            self.run_async_task("read_did", {"did": self.parse_did()})
        except ValueError:
            self.log_error("Ungültiges DID-Format.")

    def start_write_did(self):
        try:
            did = self.parse_did()
            data_bytes = [int(x, 16) for x in self.txt_did_data.text().split() if x]
            self.run_async_task("write_did", {"did": did, "data": data_bytes})
        except ValueError:
            self.log_error("Ungültige DID- oder Schreibdaten-Bytes.")

    def log_output(self, direction: str, payload: str):
        color_map = {"TX": "#0000FF", "RX": "#008000", "SYS": "#808080"}
        color = color_map.get(direction, "#000000")
        timestamp = time.strftime("%H:%M:%S")
        html = f"<span style='color:gray;'>[{timestamp}]</span> <b style='color:{color};'>[{direction}]</b> {payload}"
        self.log_view.append(html)

    def log_error(self, err_msg: str):
        timestamp = time.strftime("%H:%M:%S")
        html = f"<span style='color:gray;'>[{timestamp}]</span> <b style='color:#FF0000;'>[ERROR]</b> {err_msg}"
        self.log_view.append(html)

    def closeEvent(self, event):
        self.engine.disconnect()
        event.accept()

if __name__ == "__main__":
    app = QApplication(sys.argv)
    gui = UDSHardwareTesterGUI()
    gui.show()
    sys.exit(app.exec_())