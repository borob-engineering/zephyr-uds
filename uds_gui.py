#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
from PyQt5.QtWidgets import (QApplication, QWidget, QVBoxLayout, QHBoxLayout, 
                             QPushButton, QTextEdit, QGroupBox, QFileDialog, QLabel)
from PyQt5.QtCore import Qt

class UdsGui(QWidget):
    def __init__(self, socket=None):
        """
        Grafische Test-Oberfläche für die Verifikation des Zephyr UDS-Servers.
        Arbeitet rein hardwarebasiert ohne lokale Simulation.
        """
        super().__init__()
        self.socket = socket
        self.gui_block_sequence_counter = 1  # Lokaler Zähler für Service 0x36
        self.init_ui()

    def init_ui(self):
        self.setWindowTitle("UDS Hardware-Testsuite (ISO 14229-1)")
        self.resize(650, 700)

        main_layout = QVBoxLayout()

        # --- Gruppe 1: Session & Basic Diagnostics ---
        basic_group = QGroupBox("Diagnostic Sessions & Security (0x10 / 0x27 / 0x11)")
        basic_layout = QHBoxLayout()
        
        self.btn_default_sess = QPushButton("Default (0x01)")
        self.btn_default_sess.clicked.connect(lambda: self.send_raw_request(bytes([0x10, 0x01])))
        
        self.btn_prog_sess = QPushButton("Programming (0x02)")
        self.btn_prog_sess.clicked.connect(lambda: self.send_raw_request(bytes([0x10, 0x02])))
        
        self.btn_ext_sess = QPushButton("Extended (0x03)")
        self.btn_ext_sess.clicked.connect(lambda: self.send_raw_request(bytes([0x10, 0x03])))

        self.btn_req_seed = QPushButton("Request Seed (0x27)")
        self.btn_req_seed.clicked.connect(lambda: self.send_raw_request(bytes([0x27, 0x01])))

        self.btn_ecu_reset = QPushButton("ECU Reset (0x11)")
        self.btn_ecu_reset.clicked.connect(lambda: self.send_raw_request(bytes([0x11, 0x01])))

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

        self.btn_read_dtc = QPushButton("Read DTCs (reportDTCByStatusMask)")
        self.btn_read_dtc.clicked.connect(lambda: self.send_raw_request(bytes([0x19, 0x02, 0xFF])))

        self.btn_clear_dtc = QPushButton("Clear Fault Memory (All DTCs)")
        self.btn_clear_dtc.clicked.connect(lambda: self.send_raw_request(bytes([0x14, 0xFF, 0xFF, 0xFF])))

        dtc_layout.addWidget(self.btn_read_dtc)
        dtc_layout.addWidget(self.btn_clear_dtc)
        dtc_group.setLayout(dtc_layout)
        main_layout.addWidget(dtc_group)

        # --- Gruppe 3: E/A Control & Routines ---
        hw_group = QGroupBox("Hardware I/O & Routine Control (0x2F / 0x31)")
        hw_layout = QHBoxLayout()

        self.btn_io_active = QPushButton("Actuator Short-Term Adj (0x2F)")
        self.btn_io_active.clicked.connect(lambda: self.send_raw_request(bytes([0x2F, 0xDF, 0x01, 0x03, 0x01])))

        self.btn_io_release = QPushButton("Return Control To ECU (0x2F)")
        self.btn_io_release.clicked.connect(lambda: self.send_raw_request(bytes([0x2F, 0xDF, 0x01, 0x00])))

        self.btn_start_routine = QPushButton("Start System Routine 0x0202")
        self.btn_start_routine.clicked.connect(lambda: self.send_raw_request(bytes([0x31, 0x01, 0x02, 0x02])))

        hw_layout.addWidget(self.btn_io_active)
        hw_layout.addWidget(self.btn_io_release)
        hw_layout.addWidget(self.btn_start_routine)
        hw_group.setLayout(hw_layout)
        main_layout.addWidget(hw_group)

        # --- Gruppe 4: Firmware Flashing Pipeline ---
        flash_group = QGroupBox("Firmware Flashing Pipeline Automation (0x34 -> 0x36 -> 0x37)")
        flash_layout = QVBoxLayout()

        self.lbl_flash_status = QLabel("Keine Firmware-Datei geladen.")
        self.lbl_flash_status.setAlignment(Qt.AlignCenter)
        
        self.btn_flash_stream = QPushButton("Firmware-Binary (.bin) auswählen und streamen")
        self.btn_flash_stream.clicked.connect(self.action_automate_flash_pipeline)

        flash_layout.addWidget(self.lbl_flash_status)
        flash_layout.addWidget(self.btn_flash_stream)
        flash_group.setLayout(flash_layout)
        main_layout.addWidget(flash_group)

        # --- Text-Logbereich mit Überschrift und Löschfunktion ---
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
        """Hängt Text an den Logbereich an."""
        self.log_area.append(text)
    def send_raw_request(self, payload):
        """Sendet Daten direkt an das ISO-TP-Socket und übersetzt das TX-Protokoll im Log."""
        if not payload or not self.socket:
            self.log("[Fehler] Kein aktives CAN/ISO-TP-Interface vorhanden!")
            return

        sid = payload[0]
        explanation = "Unbekannter Dienst"
        
        # --- ISO 14229-1 Dienst-Übersetzer für ausgehende Nachrichten (TX) ---
        if sid == 0x10:
            sub = payload[1] if len(payload) > 1 else 0
            sub_name = {1: "Default", 2: "Programming", 3: "Extended"}.get(sub, f"0x{sub:02X}")
            explanation = f"DiagnosticSessionControl -> Wechsel in {sub_name} Session"
        elif sid == 0x11:
            explanation = "ECUReset -> Steuergeräte-Neustart anfordern"
        elif sid == 0x14:
            explanation = "ClearDiagnosticInformation -> Fehlerspeicher löschen"
        elif sid == 0x19:
            explanation = "ReadDTCInformation -> Fehlerspeicher auslesen (DTCs)"
        elif sid == 0x22:
            explanation = f"ReadDataByIdentifier -> DID 0x{payload[1:3].hex().upper()} lesen"
        elif sid == 0x27:
            sub = payload[1] if len(payload) > 1 else 0
            explanation = f"SecurityAccess -> Seed anfordern (Sub 0x01)" if sub == 0x01 else f"SecurityAccess -> Key senden (Sub 0x02)"
        elif sid == 0x2E:
            explanation = f"WriteDataByIdentifier -> DID 0x{payload[1:3].hex().upper()} beschreiben"
        elif sid == 0x2F:
            param = payload[3] if len(payload) > 3 else 0
            param_name = "ReturnControl" if param == 0x00 else "ShortTermAdjustment"
            explanation = f"InputOutputControl -> LED/Aktuator steuern ({param_name})"
        elif sid == 0x31:
            explanation = f"RoutineControl -> Routine 0x{payload[2:4].hex().upper()} starten"
        elif sid == 0x34:
            explanation = "RequestDownload -> Firmware-Pipeline initialisieren (Startbefehl)"
        elif sid == 0x36:
            bsc = payload[1] if len(payload) > 1 else 0
            explanation = f"TransferData -> Sende Firmware-Block #{bsc} ({len(payload)-2} Bytes)"
        elif sid == 0x37:
            explanation = "RequestTransferExit -> Firmware-Übertragung beenden (Abschlussbefehl)"

        self.log(f"[TX ISO-TP] {payload.hex().upper()} ({explanation})")
        
        try:
            # Physikalisch auf die CAN-Leitung ausgeben
            self.socket.send(payload)
        except Exception as e:
            self.log(f"[ISO-TP Fehler] Senden fehlgeschlagen: {e}")

    def gui_receive_logger(self, tx_data):
        """Zentraler Empfangs-Logger für eintreffende Hardware-Antworten."""
        if not tx_data:
            return
        
        resp_sid = tx_data[0]
        resp_explain = "Unbekannte Antwort"
        
        # --- ISO 14229-1 Dienst-Übersetzer für eingehende Antworten (RX) ---
        if resp_sid == 0x7F:
            # Negative Response (Fehlerfall)
            failed_sid_byte = tx_data[1] if len(tx_data) > 1 else 0
            failed_sid = f"0x{failed_sid_byte:02X}"
            nrc_code = tx_data[2] if len(tx_data) > 2 else 0
            nrc_messages = {
                0x11: "ServiceNotSupported (Dienst nicht unterstützt)",
                0x12: "SubFunctionNotSupported (Unterfunktion nicht unterstützt)",
                0x13: "IncorrectMessageLengthOrInvalidFormat (Falsche Payload-Länge)",
                0x22: "ConditionsNotCorrect (Befehl in dieser Session blockiert)",
                0x24: "RequestSequenceError (Falsche Reihenfolge der Befehle)",
                0x31: "RequestOutOfRange (Daten-ID / DID nicht gefunden)",
                0x35: "InvalidKey (Sicherheitsschlüssel mathematisch falsch)",
                0x36: "ExceededNumberOfAttempts (Anti-Brute-Force Sperre aktiv)",
                0x73: "WrongBlockSequenceCounter (Falscher Blockzähler beim Flashen)",
                0x7E: "SubFunctionNotSupportedInActiveSession (In dieser Session gesperrt)"
            }
            nrc_text = nrc_messages.get(nrc_code, f"Fehlercode 0x{nrc_code:02X}")
            resp_explain = f"NRC (Negative Response) für Dienst {failed_sid}: {nrc_text}"
        
        else:
            # Positive Response (Erfolgsfall: Antwort-SID ist immer Anfrage-SID + 0x40)
            original_sid = resp_sid - 0x40
            if original_sid == 0x10:
                resp_explain = "Erfolg: Sitzungswechsel vom Steuergerät bestätigt"
            elif original_sid == 0x11:
                resp_explain = "Erfolg: Reset-Befehl akzeptiert, ECU startet neu"
            elif original_sid == 0x14:
                resp_explain = "Erfolg: Fehlerspeicher wurde geleert"
            elif original_sid == 0x19:
                resp_explain = "Erfolg: DTCs empfangen"
            elif original_sid == 0x22:
                resp_explain = f"Erfolg: Daten für DID gelesen -> Wert: {tx_data[3:].hex().upper()}"
            elif original_sid == 0x27:
                sub = tx_data[1] if len(tx_data) > 1 else 0
                if sub == 0x01:
                    seed_bytes = tx_data[2:]
                    resp_explain = f"Erfolg: Seed erhalten -> {seed_bytes.hex().upper()}"
                    # Automatisierter Handshake: Berechne den Key direkt via XOR 0xFF und schicke ihn ab
                    computed_key = bytes([b ^ 0xFF for b in seed_bytes])
                    self.log(f"[INFO] Berechne Key via XOR 0xFF -> {computed_key.hex().upper()}")
                    self.send_raw_request(bytes([0x27, 0x02]) + computed_key)
                else:
                    resp_explain = "Erfolg: ECU entsperrt (Security Access granted)"
            elif original_sid == 0x2E:
                resp_explain = "Erfolg: Daten erfolgreich auf DID geschrieben"
            elif original_sid == 0x2F:
                resp_explain = "Erfolg: Stellglied-Steuerung übernommen/quittiert"
            elif original_sid == 0x31:
                resp_explain = "Erfolg: Routine-Ausführung gestartet"
            elif original_sid == 0x34:
                resp_explain = "Erfolg: Download bereit, ECU wartet auf Datenblöcke"
            elif original_sid == 0x36:
                block_num = tx_data[1] if len(tx_data) > 1 else 0
                resp_explain = f"Erfolg: Block #{block_num} im Steuergerät gespeichert"
            elif original_sid == 0x37:
                resp_explain = "Erfolg: Flash-Pipeline geschlossen, Firmware-Update beendet"

        self.log(f"[RX ISO-TP] {tx_data.hex().upper()} ({resp_explain})\n")
    def action_automate_flash_pipeline(self):
        """Öffnet eine Datei und streamt sie vollautomatisiert über ISO-TP an das Steuergerät."""
        file_path, _ = QFileDialog.getOpenFileName(self, "Firmware-Binary öffnen", "", "Binary Files (*.bin);;All Files (*)")
        if not file_path:
            return

        try:
            with open(file_path, "rb") as f:
                bin_data = f.read()
            
            total_bytes = len(bin_data)
            self.lbl_flash_status.setText(f"Datei geladen: {total_bytes} Bytes. Flashing läuft...")
            self.log(f"[Flash] Pipeline gestartet für '{file_path}' ({total_bytes} Bytes)")

            # Schritt 1: Request Download (0x34) an die echte Hardware funken
            self.log("[Flash Step 1] Sende Request Download (0x34)...")
            self.send_raw_request(bytes([0x34, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00]))
            
            # Kurze Pause für den Bus
            time.sleep(0.1)

            # Schritt 2: Transfer Data blockweise senden (0x36)
            block_size = 256
            self.log(f"[Flash Step 2] Streamen der Daten in {block_size}-Byte Blöcken...")
            
            self.gui_block_sequence_counter = 1
            for i in range(0, total_bytes, block_size):
                chunk = bin_data[i:i+block_size]
                payload = bytes([0x36, self.gui_block_sequence_counter]) + chunk
                self.send_raw_request(payload)
                
                # Erhöhe den Zähler rollierend im Wertebereich 1-255 nach ISO-Standard
                self.gui_block_sequence_counter = (self.gui_block_sequence_counter + 1) if self.gui_block_sequence_counter < 255 else 1
                time.sleep(0.01) # Schreibpause für den Flash-Speicher des Zephyr-Knotens

            # Schritt 3: Request Transfer Exit (0x37)
            self.log("[Flash Step 3] Schließe Übertragung mit Request Transfer Exit (0x37)...")
            self.send_raw_request(bytes([0x37]))
            self.lbl_flash_status.setText("Flash-Vorgang abgeschlossen.")

        except Exception as e:
            self.log(f"[Flash Exception] Kritischer Fehler beim Flashen: {e}")
            self.lbl_flash_status.setText("Fehler beim Datei-Streaming.")


# ==============================================================================
# ISO-TP HARDWARE APP RUNNER (Isolierter Direktstart-Fallback)
# ==============================================================================
if __name__ == "__main__":
    import isotp
    print("[Direktstart] Initialisiere Hardware-Modus auf Standard-Interface 'can0'...")
    
    app = QApplication(sys.argv)
    socket = isotp.socket(timeout=1.0)
    try:
        socket.set_opts(txpad=0xAA, rxpad=0xAA)
        socket.set_fc_opts(bs=0, stmin=0)
        socket.bind("can0", isotp.Address(rxid=0x7E8, txid=0x7E0))
        
        gui = UdsGui(socket=socket)
        gui.show()
        sys.exit(app.exec_())
    except Exception as e:
        print(f"[Fatal] Interface 'can0' blockiert oder nicht vorhanden: {e}")
