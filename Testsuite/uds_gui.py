#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QPushButton, 
                             QTextEdit, QGroupBox, QFileDialog, QLabel)
from PyQt5.QtCore import Qt, pyqtSignal
from udsoncan import services

class UdsGui(QWidget):
    # Dediziertes Qt-Signal für thread-sicheres Logging
    rx_packet_signal = pyqtSignal(bytes)

    def __init__(self, uds_client=None, socket=None):
        super().__init__()
        self.client = uds_client
        self.socket = socket
        self.gui_block_sequence_counter = 1
        
        # Signal mit der Empfangsmethode verknüpfen
        self.rx_packet_signal.connect(self.gui_receive_logger)
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("UDS Hardware-Testsuite (udsoncan-Engine)")
        self.resize(650, 700)
        main_layout = QVBoxLayout()

        # --- Gruppe 1: Session & Basic Diagnostics ---
        basic_group = QGroupBox("Diagnostic Sessions & Security (0x10 / 0x27 / 0x11)")
        basic_layout = QHBoxLayout()
        self.btn_default_sess = QPushButton("Default (0x01)")
        self.btn_default_sess.clicked.connect(lambda: self.execute_uds_call(bytes([0x10, 0x01])))
        self.btn_prog_sess = QPushButton("Programming (0x02)")
        self.btn_prog_sess.clicked.connect(lambda: self.execute_uds_call(bytes([0x10, 0x02])))
        self.btn_ext_sess = QPushButton("Extended (0x03)")
        self.btn_ext_sess.clicked.connect(lambda: self.execute_uds_call(bytes([0x10, 0x03])))
        self.btn_req_seed = QPushButton("Security Handshake (0x27)")
        self.btn_req_seed.clicked.connect(lambda: self.execute_uds_call(bytes([0x27, 0x01])))
        self.btn_ecu_reset = QPushButton("ECU Reset (0x11)")
        self.btn_ecu_reset.clicked.connect(lambda: self.execute_uds_call(bytes([0x11, 0x01])))
        basic_layout.addWidget(self.btn_default_sess)
        basic_layout.addWidget(self.btn_prog_sess)
        basic_layout.addWidget(self.btn_ext_sess)
        basic_layout.addWidget(self.btn_req_seed)
        basic_layout.addWidget(self.btn_ecu_reset)
        basic_group.setLayout(basic_layout)
        main_layout.addWidget(basic_group)

        # --- Gruppe 2: DTC-Speicherverwaltung ---
        dtc_group = QGroupBox("Fehlerspeicher / DTC Management (0x14 / 0x19)")
        dtc_layout = QHBoxLayout()
        self.btn_read_dtc = QPushButton("Read DTCs (0x19)")
        self.btn_read_dtc.clicked.connect(lambda: self.execute_uds_call(bytes([0x19, 0x02, 0xFF])))
        self.btn_clear_dtc = QPushButton("Clear Fault Memory (0x14)")
        self.btn_clear_dtc.clicked.connect(lambda: self.execute_uds_call(bytes([0x14, 0xFF, 0xFF, 0xFF])))
        dtc_layout.addWidget(self.btn_read_dtc)
        dtc_layout.addWidget(self.btn_clear_dtc)
        dtc_group.setLayout(dtc_layout)
        main_layout.addWidget(dtc_group)

        # --- Gruppe 3: E/A Control & Routines ---
        hw_group = QGroupBox("Hardware I/O & Routine Control (0x2F / 0x31)")
        hw_layout = QHBoxLayout()
        self.btn_io_active = QPushButton("Actuator Control ON (0x2F)")
        self.btn_io_active.clicked.connect(lambda: self.execute_uds_call(bytes([0x2F, 0xDF, 0x01, 0x03, 0x01])))
        self.btn_io_release = QPushButton("Return Control to ECU (0x2F)")
        self.btn_io_release.clicked.connect(lambda: self.execute_uds_call(bytes([0x2F, 0xDF, 0x01, 0x00])))
        self.btn_start_routine = QPushButton("Start Routine 0x0202 (0x31)")
        self.btn_start_routine.clicked.connect(lambda: self.execute_uds_call(bytes([0x31, 0x01, 0x02, 0x02])))
        hw_layout.addWidget(self.btn_io_active)
        hw_layout.addWidget(self.btn_io_release)
        hw_layout.addWidget(self.btn_start_routine)
        hw_group.setLayout(hw_layout)
        main_layout.addWidget(hw_group)

        # --- Gruppe 4: Firmware Flashing Pipeline ---
        flash_group = QGroupBox("Firmware Flashing Pipeline (0x34 -> 0x36 -> 0x37)")
        flash_layout = QVBoxLayout()
        self.lbl_flash_status = QLabel("Keine Firmware-Datei geladen.")
        self.lbl_flash_status.setAlignment(Qt.AlignCenter)
        self.btn_flash_stream = QPushButton("Firmware (.bin) auswählen und flashen")
        self.btn_flash_stream.clicked.connect(self.action_automate_flash_pipeline)
        flash_layout.addWidget(self.lbl_flash_status)
        flash_layout.addWidget(self.btn_flash_stream)
        flash_group.setLayout(flash_layout)
        main_layout.addWidget(flash_group)

        # --- Text-Logbereich ---
        log_header_layout = QHBoxLayout()
        log_header_layout.addWidget(QLabel("UDS Bus-Überwachung & Logausgabe:"))
        self.btn_clear_log = QPushButton("Log löschen")
        self.btn_clear_log.setFixedWidth(100)
        self.btn_clear_log.clicked.connect(lambda: self.log_area.clear())
        log_header_layout.addWidget(self.btn_clear_log, alignment=Qt.AlignRight)
        main_layout.addLayout(log_header_layout)
        
        self.log_area = QTextEdit()
        self.log_area.setReadOnly(True)
        main_layout.addWidget(self.log_area)
        self.setLayout(main_layout)

    def log(self, text):
        self.log_area.append(text)

    def trigger_rx_signal(self, raw_bytes):
        self.rx_packet_signal.emit(raw_bytes)

    def execute_uds_call(self, payload):
        """Sendet Daten direkt asynchron über das ISO-TP-Socket."""
        if not self.socket:
            self.log("[Fehler] Kein Socket vorhanden!")
            return
        
        sid = payload[0]
        explain = f"UDS Dienst 0x{sid:02X}"
        if sid == 0x10: explain = "DiagnosticSessionControl"
        elif sid == 0x11: explain = "ECUReset"
        elif sid == 0x14: explain = "ClearDiagnosticInformation"
        elif sid == 0x19: explain = "ReadDTCInformation"
        elif sid == 0x27: explain = "SecurityAccess (Seed Request)"
        elif sid == 0x2F: explain = "InputOutputControlByIdentifier"
        elif sid == 0x31: explain = "RoutineControl"

        self.log(f"[TX ISO-TP] {payload.hex().upper()} ({explain})")
        try:
            self.socket.send(payload)
        except Exception as e:
            self.log(f"[Fehler] Senden fehlgeschlagen: {e}")

    def gui_receive_logger(self, raw_bytes):
        """Wird aufgerufen, sobald der Hintergrundthread Daten empfängt."""
        if not raw_bytes:
            return
        
        resp_sid = raw_bytes[0]
        resp_explain = "Antwort erhalten"
        
        # Decodierung der wichtigsten UDS-Antworten und NRCs
        if resp_sid == 0x7F:
            failed_sid = raw_bytes[1] if len(raw_bytes) > 1 else 0
            nrc_code = raw_bytes[2] if len(raw_bytes) > 2 else 0
            nrc_messages = {
                0x11: "ServiceNotSupported", 0x12: "SubFunctionNotSupported",
                0x13: "IncorrectMessageLengthOrInvalidFormat", 0x24: "RequestSequenceError",
                0x31: "RequestOutOfRange", 0x35: "InvalidKey", 0x7E: "SubFunctionNotSupportedInActiveSession"
            }
            resp_explain = f"NRC für Dienst 0x{failed_sid:02X}: {nrc_messages.get(nrc_code, f'Fehler 0x{nrc_code:02X}')}"
        elif resp_sid == 0x50:
            resp_explain = f"Sitzungswechsel von ECU bestätigt (Typ 0x{raw_bytes[1]:02X})"
        elif resp_sid == 0x51:
            resp_explain = "ECU Reset akzeptiert, System startet neu"
        elif resp_sid == 0x54:
            resp_explain = "Fehlerspeicher erfolgreich geleert"
        elif resp_sid == 0x59:
            resp_explain = "DTC Fehlerliste empfangen"
        elif resp_sid == 0x67:
            sub = raw_bytes[1] if len(raw_bytes) > 1 else 0
            if sub == 0x01:
                seed = raw_bytes[2:]
                resp_explain = f"Seed erhalten: {seed.hex().upper()} -> Berechne Key..."
                computed_key = bytes([b ^ 0xFF for b in seed])
                # Schicke berechneten Key sofort hinterher (Subfunktion 0x02)
                self.socket.send(bytes([0x27, 0x02]) + computed_key)
            else:
                resp_explain = "ECU erfolgreich entsperrt (Security Access granted)"

        self.log(f"[RX ISO-TP] {raw_bytes.hex().upper()} ({resp_explain})\n")

    def action_automate_flash_pipeline(self):
        if not self.socket: return
        file_path, _ = QFileDialog.getOpenFileName(self, "Firmware-Binary öffnen", "", "Binary Files (*.bin);;All Files (*)")
        if not file_path: return
        try:
            with open(file_path, "rb") as f: bin_data = f.read()
            total_bytes = len(bin_data)
            self.lbl_flash_status.setText(f"Flashing läuft...")
            
            # 1. Download anfordern
            self.socket.send(bytes([0x34, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00]))
            time.sleep(0.1)
            
            # 2. Datenblöcke streamen
            block_size = 256
            block_counter = 1
            for i in range(0, total_bytes, block_size):
                chunk = bin_data[i:i+block_size]
                self.socket.send(bytes([0x36, block_counter]) + chunk)
                block_counter = (block_counter + 1) if block_counter < 255 else 1
                time.sleep(0.01)
            
            # 3. Pipeline schließen
            self.socket.send(bytes([0x37]))
            self.lbl_flash_status.setText("Flash-Vorgang erfolgreich!")
        except Exception as e:
            self.log(f"[Flash Error] {e}")
            self.lbl_flash_status.setText("Fehler.")

if __name__ == "__main__":
    print("Bitte starte das Tool über 'python3 main.py'!")
