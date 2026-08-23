#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import random
import can
import isotp
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QPushButton, QTextEdit, QGroupBox, 
                             QProgressBar, QLabel)
from PyQt5.QtGui import QFont, QTextCursor, QColor
from PyQt5.QtCore import Qt, QTimer, QThread, pyqtSignal

# ==============================================================================
# CAN & ISO-TP HARDWARE NETZWERKSCHICHT (TIMEOUT-FIX GEGEN ENDLOSSCHLEIFE)
# ==============================================================================
try:
    interface_name = 'slcan0'
    can_bus = can.interface.Bus(interface='socketcan', channel=interface_name, bitrate=500000)
    tp_address = isotp.Address(rxid=0x7E8, txid=0x7E0)
    
    # 1. Socket ohne Parameter erstellen
    isotp_socket = isotp.socket()
    
    # 2. Kernel-Level Timeout auf 1.0 Sekunde setzen (Löst das Blockieren)
    isotp_socket.settimeout(1.0)
    
    # 3. An das slcan0-Interface binden
    isotp_socket.bind(interface_name, tp_address)
    HARDWARE_AVAILABLE = True
except Exception as e:
    print(f"WARNUNG: CAN-Hardware konnte nicht initialisiert werden ({e}).")
    print("Das Skript wird im GUI-Demomodus gestartet.")
    HARDWARE_AVAILABLE = False


# ==============================================================================
# ASYNC WORKER FOR REAL HARDWARE FLASH PIPELINE
# ==============================================================================
class FlashWorker(QThread):
    progress_sig = pyqtSignal(int)
    log_sig = pyqtSignal(str, str) # text, type

    def run(self):
        if not HARDWARE_AVAILABLE:
            self.log_sig.emit("Flash abgebrochen: Keine CAN-Hardware verfügbar!", "err")
            return

        self.log_sig.emit("Starte reale UDS-Flashing-Prozedur via CAN...", "info")
        
        # Schritt 1: Sessionwechsel zu Programming (0x10 0x02)
        tx = bytes([0x10, 0x02])
        self.log_sig.emit(f"TX (CAN): {tx.hex().upper()}", "tx")
        isotp_socket.send(tx)
        
        try:
            rx = isotp_socket.recv()
            if not rx or rx[0] != 0x50:
                self.log_sig.emit(f"Flash-Abbruch: Sessionwechsel verweigert! RX: {rx.hex().upper() if rx else 'None'}", "err")
                return
            self.log_sig.emit(f"RX (CAN): {rx.hex().upper()} (PROGRAMMING_SESSION aktiv)", "rx")
        except TimeoutError:
            self.log_sig.emit("Flash-Abbruch: Timeout bei Sessionwechsel!", "err")
            return
        
        time.sleep(0.1)

        # Schritt 2: Request Download (0x34)
        tx = bytes([0x34, 0x00, 0x44, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0xFF])
        self.log_sig.emit(f"TX (CAN): {tx.hex().upper()}", "tx")
        isotp_socket.send(tx)
        
        try:
            rx = isotp_socket.recv()
            if not rx or rx[0] != 0x74:
                self.log_sig.emit(f"Flash-Abbruch: Download-Anfrage verweigert! RX: {rx.hex().upper() if rx else 'None'}", "err")
                return
            self.log_sig.emit(f"RX (CAN): {rx.hex().upper()} (Download-Kanal offen)", "rx")
        except TimeoutError:
            self.log_sig.emit("Flash-Abbruch: Timeout bei Request Download!", "err")
            return
        
        time.sleep(0.1)

        # Schritt 3: Transfer Data (0x36) - Sende 10 Blöcke über ISO-TP
        for bsc in range(1, 11):
            dummy_payload = bytes([random.randint(0, 255) for _ in range(8)])
            tx = bytes([0x36, bsc]) + dummy_payload
            self.log_sig.emit(f"TX (CAN): Block {bsc} -> {tx[:4].hex().upper()}...", "tx")
            isotp_socket.send(tx)
            
            try:
                rx = isotp_socket.recv() # Höheres Timeout für Flash-Schreibvorgang
                if not rx or rx[0] != 0x66:
                    self.log_sig.emit(f"Flash-Abbruch: Fehler bei Block {bsc}! RX: {rx.hex().upper() if rx else 'None'}", "err")
                    return
                self.progress_sig.emit(int(bsc * 10))
            except TimeoutError:
                self.log_sig.emit(f"Flash-Abbruch: Timeout bei Block {bsc}!", "err")
                return
            time.sleep(0.02)

        # Schritt 4: Request Transfer Exit (0x37)
        tx = bytes([0x37])
        self.log_sig.emit(f"TX (CAN): {tx.hex().upper()}", "tx")
        isotp_socket.send(tx)
        
        try:
            rx = isotp_socket.recv()
            if not rx or rx[0] != 0x77:
                self.log_sig.emit("Flash-Abbruch: Exit vom Steuergerät nicht quittiert!", "err")
                return
            self.log_sig.emit(f"RX (CAN): {rx.hex().upper()} (Streaming geschlossen)", "rx")
            self.log_sig.emit("Firmware Update erfolgreich auf Zephyr-Knoten geflasht!", "success")
        except TimeoutError:
            self.log_sig.emit("Flash-Abbruch: Timeout bei Transfer Exit!", "err")
# ==============================================================================
# GRAPHICAL HARDWARE TEST INTERFACE
# ==============================================================================
class ZephyrUDSHardwareTester(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # ZUERST VARIABLEN INITIALISIEREN (Verhindert den AttributeError)
        self.active_session_type = "Unbekannt (Warte auf Abfrage)"
        self.security_status = "Gesperrt / Unbekannt"
        
        # JETZT DIE OBERFLÄCHE BAUEN
        self.initUI()
        
        # ZULETZT DEN TIMER STARTEN
        self.gui_update_timer = QTimer()
        self.gui_update_timer.timeout.connect(self.update_state_labels)
        self.gui_update_timer.start(500)

    def initUI(self):
        self.setWindowTitle('Zephyr RTOS - Real Hardware UDS CAN Test-Suite')
        self.setGeometry(100, 100, 950, 650)
        
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QHBoxLayout(central_widget)
        left_layout = QVBoxLayout()
        
        # Gruppe 1: Live Status Anzeige
        status_group = QGroupBox("Erkanntes Steuergerät-Zustandsbild")
        status_grid = QVBoxLayout()
        self.lbl_session = QLabel(f"Aktive Session: {self.active_session_type}")
        self.lbl_security = QLabel(f"Security Level: {self.security_status}")
        status_grid.addWidget(self.lbl_session)
        status_grid.addWidget(self.lbl_security)
        status_group.setLayout(status_grid)
        left_layout.addWidget(status_group)

        # Gruppe 2: Standard UDS Dienste
        basic_group = QGroupBox("Standard UDS Services (ISO 14229-1)")
        basic_box = QVBoxLayout()
        
        btn_session_ext = QPushButton("0x10 0x03: Switch to EXTENDED SESSION")
        btn_session_ext.clicked.connect(lambda: self.send_hardware_request(bytes([0x10, 0x03])))
        
        btn_tester_present = QPushButton("0x3E 0x00: Send TESTER PRESENT (Keep-Alive)")
        btn_tester_present.clicked.connect(lambda: self.send_hardware_request(bytes([0x3E, 0x00])))
        
        btn_read_vin = QPushButton("0x22 0xF1 0x90: Read DID 0xF190 (VIN)")
        btn_read_vin.clicked.connect(lambda: self.send_hardware_request(bytes([0x22, 0xF1, 0x90])))
        
        btn_clear_dtc = QPushButton("0x14: Clear DTC Information")
        btn_clear_dtc.clicked.connect(lambda: self.send_hardware_request(bytes([0x14, 0xFF, 0xFF, 0xFF])))

        btn_read_dtc = QPushButton("0x19 0x02: Read DTC By Status Mask")
        btn_read_dtc.clicked.connect(lambda: self.send_hardware_request(bytes([0x19, 0x02, 0xFF])))

        btn_ecu_reset = QPushButton("0x11 0x01: Trigger ECU Hard Reset")
        btn_ecu_reset.clicked.connect(lambda: self.send_hardware_request(bytes([0x11, 0x01])))

        basic_box.addWidget(btn_session_ext)
        basic_box.addWidget(btn_tester_present)
        basic_box.addWidget(btn_read_vin)
        basic_box.addWidget(btn_clear_dtc)
        basic_box.addWidget(btn_read_dtc)
        basic_box.addWidget(btn_ecu_reset)
        basic_group.setLayout(basic_box)
        left_layout.addWidget(basic_group)

        # Gruppe 3: Erweiterte Dienste & Flashing
        adv_group = QGroupBox("Security Access & Flash Engine")
        adv_box = QVBoxLayout()
        
        btn_req_seed = QPushButton("0x27 0x01: Automated Seed/Key Challenge")
        btn_req_seed.clicked.connect(self.handle_security_seed_request)
        
        btn_send_wrong_key = QPushButton("0x27 0x02: Send Invalid Key (Lockout Test)")
        btn_send_wrong_key.clicked.connect(lambda: self.send_hardware_request(bytes([0x27, 0x02, 0x00, 0x00, 0x00, 0x00])))

        btn_write_did = QPushButton("0x2E: Write Data By Identifier")
        btn_write_did.clicked.connect(lambda: self.send_hardware_request(bytes([0x2E, 0xF1, 0x90, 0xAA, 0xBB])))

        self.btn_flash = QPushButton("Simulate Full Flashing Sequence (0x34/0x36/0x37)")
        self.btn_flash.setStyleSheet("background-color: #2b579a; color: white; font-weight: bold;")
        self.btn_flash.clicked.connect(self.trigger_flash_sequence)
        
        self.progress_bar = QProgressBar()
        self.progress_bar.setValue(0)

        adv_box.addWidget(btn_req_seed)
        adv_box.addWidget(btn_send_wrong_key)
        adv_box.addWidget(btn_write_did)
        adv_box.addWidget(self.btn_flash)
        adv_box.addWidget(self.progress_bar)
        adv_group.setLayout(adv_box)
        left_layout.addWidget(adv_group)

        # Rechte Spalte: Terminal Monitor
        right_layout = QVBoxLayout()
        terminal_group = QGroupBox("CAN / ISO-TP Traffic Realtime Console Monitor")
        terminal_box = QVBoxLayout()
        
        self.terminal = QTextEdit()
        self.terminal.setReadOnly(True)
        self.terminal.setFont(QFont("Courier New", 10))
        self.terminal.setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;")
        
        btn_clear_log = QPushButton("Terminal-Log löschen")
        btn_clear_log.clicked.connect(self.terminal.clear)
        
        terminal_box.addWidget(self.terminal)
        terminal_box.addWidget(btn_clear_log)
        terminal_group.setLayout(terminal_box)
        right_layout.addWidget(terminal_group)

        main_layout.addLayout(left_layout, stretch=2)
        main_layout.addLayout(right_layout, stretch=3)
        
        if HARDWARE_AVAILABLE:
            self.log_message("Verbindung zu 'can0' erfolgreich aufgebaut. Bereit für ECU-Hardware-Test.", "success")
        else:
            self.log_message("Kein CAN-Bus gefunden. Buttons senden keine echten Daten auf die Leitung.", "err")

    def log_message(self, msg, msg_type="info"):
        cursor = self.terminal.textCursor()
        cursor.movePosition(QTextCursor.End)
        self.terminal.setTextCursor(cursor)
        
        color_map = {
            "tx": QColor("#007acc"), "rx": QColor("#b5cea8"), 
            "err": QColor("#f44336"), "success": QColor("#4caf50"), "info": QColor("#9cdcfe")
        }
        prefix_map = {"tx": "[TX] ", "rx": "[RX] ", "err": "[ERR] ", "success": "[OK] ", "info": "[SYS] "}
        
        self.terminal.setTextColor(color_map.get(msg_type, QColor("white")))
        self.terminal.insertPlainText(f"{prefix_map.get(msg_type, '')}{msg}\n")

    def send_hardware_request(self, tx_bytes):
        if not HARDWARE_AVAILABLE:
            self.log_message("Aktion abgebrochen: Kein CAN-Interface aktiv.", "err")
            return None

        self.log_message(f"TX (CAN): {tx_bytes.hex().upper()}", "tx")
        
        try:
            isotp_socket.send(tx_bytes)
        except Exception as e:
            self.log_message(f"TX-Fehler (Bus down / No ACK?): {e}", "err")
            return None
        
        try:
            # Versuche Daten zu empfangen - blockiert maximal 1 Sekunde
            rx_bytes = isotp_socket.recv()
            
            if rx_bytes and rx_bytes[0] == 0x7F:
                nrc_id = f"{rx_bytes[2]:02X}" if len(rx_bytes) > 2 else "??"
                self.log_message(f"RX (CAN): {rx_bytes.hex().upper()} -> NEGATIVE RESPONSE (NRC 0x{nrc_id})", "err")
            else:
                self.log_message(f"RX (CAN): {rx_bytes.hex().upper()} -> POSITIVE RESPONSE", "rx")
                self.evaluate_ecu_state(tx_bytes, rx_bytes)
            return rx_bytes
            
        except (TimeoutError, OSError) as e:
            # Greift sofort, wenn nach 1 Sekunde kein CAN-Knoten geantwortet hat
            self.log_message("RX (CAN): FEHLER - Keine Antwort vom Zephyr-Knoten erhalten (Timeout)!", "err")
            return None


    def handle_security_seed_request(self):
        # 1. Seed anfordern
        rx = self.send_hardware_request(bytes([0x27, 0x01]))
        
        # Prüfen, ob die Antwort eine valide positive Seed-Antwort ist
        if rx and len(rx) >= 6 and rx[0] == 0x67 and rx[1] == 0x01:
            seed = int.from_bytes(rx[2:6], 'big')
            self.log_message(f"Seed erfolgreich extrahiert: {seed:08X}", "info")
            
            # Schlüssel berechnen (Muss exakt dem Algorithmus in deiner Zephyr-ECU entsprechen)
            key = seed ^ 0xDEADBEEF
            tx_key = bytes([0x27, 0x02]) + key.to_bytes(4, 'big')
            
            # Schlüssel zeitverzögert automatisch zurücksenden
            QTimer.singleShot(400, lambda: self.send_hardware_request(tx_key))

    def evaluate_ecu_state(self, tx, rx):
        # Aktualisiert die internen Variablen basierend auf dem echten Busverkehr
        if tx[0] == 0x10 and rx[0] == 0x50:
            sessions = {0x01: "DEFAULT_SESSION", 0x02: "PROGRAMMING_SESSION", 0x03: "EXTENDED_SESSION"}
            self.active_session_type = sessions.get(tx[1], f"Custom (0x{tx[1]:02X})")
        if tx[0] == 0x27 and tx[1] == 0x02 and rx[0] == 0x67:
            self.security_status = "Freigeschaltet (Level 1)"
        if tx[0] == 0x11 and rx[0] == 0x51:
            self.active_session_type = "DEFAULT_SESSION (Nach Reset)"
            self.security_status = "Gesperrt (Level 0)"

    def trigger_flash_sequence(self):
        self.progress_bar.setValue(0)
        self.flash_worker = FlashWorker()
        self.flash_worker.progress_sig.connect(self.progress_bar.setValue)
        self.flash_worker.log_sig.connect(self.log_message)
        self.flash_worker.start()

    def update_state_labels(self):
        self.lbl_session.setText(f"Aktive Session: {self.active_session_type}")
        self.lbl_security.setText(f"Security Level: {self.security_status}")

if __name__ == '__main__':
    app = QApplication(sys.argv)
    ex = ZephyrUDSHardwareTester()
    ex.show()
    sys.exit(app.exec_())
