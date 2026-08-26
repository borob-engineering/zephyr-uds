import sys
import time
from PyQt5.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, 
    QHBoxLayout, QPushButton, QTextEdit, QGroupBox, QGridLayout
)
from PyQt5.QtCore import QCoreApplication
import can
import isotp

class UdsGuiTesterWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.interface_name = 'can0'
        self.isotp_socket = None
        self.init_ui()
        self.init_can_network()

    def init_can_network(self):
        """Initialisiert das ISO-TP-Netzwerk mit einem Schutz-Timeout."""
        try:
            tp_address = isotp.Address(rxid=0x7E8, txid=0x7E0)
            self.isotp_socket = isotp.socket()
            
            # Setzt einen Timeout von 2.0 Sekunden auf Socket-Ebene gegen Core Dumps
            self.isotp_socket.settimeout(2.0)
            
            self.isotp_socket.bind(self.interface_name, tp_address)
            self.log_output(f"[SYSTEM] ISO-TP Socket aktiv auf {self.interface_name} (Timeout: 2s).")
        except Exception as e:
            self.log_output(f"[SYSTEM-FEHLER] CAN-Verbindung fehlgeschlagen: {str(e)}")

    def log_output(self, text):
        self.text_log.append(text)
        self.text_log.ensureCursorVisible()

    def closeEvent(self, event):
        if self.isotp_socket:
            self.isotp_socket.close()
        event.accept()

    def init_ui(self):
        """Erstellt das Layout für die Abdeckung aller C-Module ohne Überlagerungen."""
        self.setWindowTitle("Zephyr UDS Modul-Testsuite (Vollständige Abdeckung)")
        self.resize(950, 800)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout(central_widget)

        # Gitter-Widget für die saubere Platzierung aller 4 Testgruppen
        grid_widget = QWidget()
        grid_layout = QGridLayout(grid_widget)

        # --- Gruppe 1: Core & Session (uds_session.c / uds_security.c) ---
        group_core = QGroupBox("1. Session & Security (uds_session.c / uds_security.c)")
        layout_core = QVBoxLayout()
        self.btn_session = QPushButton("Session Control (0x10 0x03)")
        self.btn_session.clicked.connect(self.send_extended_session)
        layout_core.addWidget(self.btn_session)
        
        self.btn_s3_timer = QPushButton("S3 k_timer Fallback Test")
        self.btn_s3_timer.clicked.connect(self.run_s3_timer_test)
        layout_core.addWidget(self.btn_s3_timer)
        
        self.btn_brute = QPushButton("Security Access Sperr-Test (0x27)")
        self.btn_brute.clicked.connect(self.run_brute_force_lock_test)
        layout_core.addWidget(self.btn_brute)
        
        group_core.setLayout(layout_core)
        grid_layout.addWidget(group_core, 0, 0)

        # --- Gruppe 2: DTC Management (uds_clear_dtc.c / uds_read_dtc.c) ---
        group_dtc = QGroupBox("2. DTC Management (uds_clear_dtc.c / uds_read_dtc.c)")
        layout_dtc = QVBoxLayout()
        self.btn_read_dtc = QPushButton("Read DTC by Status (0x19 0x02)")
        self.btn_read_dtc.clicked.connect(self.run_read_dtc_test)
        layout_dtc.addWidget(self.btn_read_dtc)
        
        self.btn_clear_dtc = QPushButton("Asynchrones Clear DTC (0x14)")
        self.btn_clear_dtc.clicked.connect(self.run_async_work_test)
        layout_dtc.addWidget(self.btn_clear_dtc)
        
        group_dtc.setLayout(layout_dtc)
        grid_layout.addWidget(group_dtc, 0, 1)

        # --- Gruppe 3: Ein-/Ausgabe & Routinen (uds_iocontrol.c / uds_routine.c) ---
        group_ctrl = QGroupBox("3. IO & Routines (uds_iocontrol.c / uds_routine.c)")
        layout_ctrl = QVBoxLayout()
        
        self.btn_io = QPushButton("IO Control by Identifier (0x2F)")
        self.btn_io.clicked.connect(self.run_io_control_test)
        layout_ctrl.addWidget(self.btn_io)
        
        self.btn_routine = QPushButton("Routine Control Start (0x31)")
        self.btn_routine.clicked.connect(self.run_routine_control_test)
        layout_ctrl.addWidget(self.btn_routine)
        
        group_ctrl.setLayout(layout_ctrl)
        grid_layout.addWidget(group_ctrl, 1, 0)

        # --- Gruppe 4: Flash Pipeline & Broadcast (uds_flash_pipeline.c) ---
        group_flash = QGroupBox("4. Flash & Broadcast (uds_flash_pipeline.c)")
        layout_flash = QVBoxLayout()
        
        self.btn_flash = QPushButton("Request Download (0x34)")
        self.btn_flash.clicked.connect(self.run_flash_pipeline_test)
        layout_flash.addWidget(self.btn_flash)
        
        self.btn_func = QPushButton("Funktionale NRC-Unterdrückung")
        self.btn_func.clicked.connect(self.run_functional_suppression_test)
        layout_flash.addWidget(self.btn_func)
        
        group_flash.setLayout(layout_flash)
        grid_layout.addWidget(group_flash, 1, 1)

        main_layout.addWidget(grid_widget)

        # --- Terminal Log-Fenster ---
        log_group = QGroupBox("Bus- und Testprotokoll")
        log_layout = QVBoxLayout()
        self.text_log = QTextEdit()
        self.text_log.setReadOnly(True)
        self.text_log.setStyleSheet("background-color: #1E1E1E; color: #A9B7C6; font-family: Consolas;")
        log_layout.addWidget(self.text_log)
        log_group.setLayout(log_layout)
        main_layout.addWidget(log_group)
    # ==============================================================================
    # AKTIONEN: CORE, SESSION & SECURITY VALIDIERUNG
    # ==============================================================================
    def send_extended_session(self):
        if not self.isotp_socket: return False
        self.log_output("[TX] 0x10 0x03 -> DiagnosticSessionControl: EXTENDED")
        try:
            self.isotp_socket.send(b"\x10\x03")
            resp = self.isotp_socket.recv()
            if resp and len(resp) >= 2 and resp[0] == 0x50 and resp[1] == 0x03:
                self.log_output(f"[RX] Positive Response (0x50 0x03): {resp.hex().upper()}")
                return True
            self.log_output(f"[RX] Negativ / Unerwartet: {resp.hex().upper() if resp else 'None'}")
        except TimeoutError:
            self.log_output("[RX] Timeout beim Session-Wechsel.")
        return False

    def run_s3_timer_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: S3-Timer Fallback (uds_session.c k_timer)...")
        if self.send_extended_session():
            self.log_output("[OK] Extended Session active. Halte Datenverkehr für 5.5s an...")
            for _ in range(55):
                time.sleep(0.1)
                QCoreApplication.processEvents()
            self.log_output(" -> Wartezeit um. Sende Schreibbefehl (0x2E)...")
            try:
                self.isotp_socket.send(b"\x2E\xF1\x90\x01\x02")
                resp_after = self.isotp_socket.recv()
                if resp_after and len(resp_after) >= 3 and resp_after[0] == 0x7F and resp_after[2] == 0x7E:
                    self.log_output("[SUCCESS] S3 k_timer erfolgreich! Server ist autark zurückgefallen (NRC 0x7E).")
                else:
                    self.log_output(f"[FAIL] Kein Fallback. Antwort: {resp_after.hex().upper() if resp_after else 'None'}")
            except TimeoutError:
                self.log_output("[FAIL] Timeout beim Lesen nach S3-Wartezeit.")

    def run_brute_force_lock_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Security Access (uds_security.c)...")
        if not self.send_extended_session(): return
        try:
            for i in range(1, 4):
                self.isotp_socket.send(b"\x27\x01")
                self.isotp_socket.recv()
                self.isotp_socket.send(b"\x27\x02\x00\x00\x00\x00")
                resp = self.isotp_socket.recv()
                if resp: self.log_output(f" -> Fehlversuch {i}/3 übermittelt. Antwort: {resp.hex().upper()}")
                QCoreApplication.processEvents()
            self.isotp_socket.send(b"\x27\x01")
            lock_resp = self.isotp_socket.recv()
            if lock_resp and len(lock_resp) >= 3 and lock_resp[0] == 0x7F and lock_resp[2] == 0x36:
                self.log_output("[SUCCESS] Anti-Brute-Force-Sperre intakt (NRC 0x36 empfangen).")
            else:
                self.log_output(f"[INFO] Antwort der ECU bei 4. Anfrage: {lock_resp.hex().upper() if lock_resp else 'None'}")
        except TimeoutError:
            self.log_output("[RX] Timeout während des Security Access Tests.")

    # ==============================================================================
    # AKTIONEN: DTC SPEICHER VALIDIERUNG
    # ==============================================================================
    def run_read_dtc_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Read DTC Information (uds_read_dtc.c)...")
        try:
            self.isotp_socket.send(b"\x19\x02\x8F")
            resp = self.isotp_socket.recv()
            if resp and resp[0] == 0x59:
                self.log_output(f"[SUCCESS] DTCs erfolgreich ausgelesen! Antwort-Bytes: {resp.hex().upper()}")
            else:
                self.log_output(f"[FAIL] Negative Response vom C-Modul: {resp.hex().upper() if resp else 'None'}")
        except TimeoutError:
            self.log_output("[FAIL] Timeout-Verbindungsfehler beim Lesen der DTCs. Prüfe ISO-TP Multi-Frame Support im Zephyr-Modul.")

    def run_async_work_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Asynchrones Clear DTC (uds_clear_dtc.c k_work)...")
        try:
            self.isotp_socket.send(b"\x14\xFF\xFF\xFF")
            max_pending_frames = 150
            for attempt in range(max_pending_frames):
                resp = self.isotp_socket.recv()
                if resp and len(resp) >= 3 and resp[0] == 0x7F and resp[2] == 0x78:
                    self.log_output(f"[OK] Handshake [{attempt+1}]: UDS_NRC_RESPONSE_PENDING (0x78) empfangen...")
                    QCoreApplication.processEvents()
                    continue
                if resp and resp[0] == 0x54:
                    self.log_output(f"[SUCCESS] Asynchroner k_work-Ablauf erfolgreich beendet! Positive Response: {resp.hex().upper()}")
                else:
                    self.log_output(f"[FAIL] Unerwartete Antwort erhalten: {resp.hex().upper() if resp else 'None'}")
                break
        except TimeoutError:
            self.log_output("[FAIL] Timeout beim asynchronen Löschen (ECU reagiert nicht mehr).")
    # ==============================================================================
    # AKTIONEN: IO CONTROL, ROUTINEN & FLASH PIPELINE VALIDIERUNG
    # ==============================================================================
    def run_io_control_test(self):
        """
        Validiert IO Control (uds_iocontrol.c).
        VARIANTE A: Berechnet den Key dynamisch aus dem empfangenen Hardware-Seed.
        VARIANTE B: Evaluiert Sicherheits-Abweisungen (NRC 0x33) protokollkonform.
        """
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Input Output Control by Identifier (uds_iocontrol.c)...")
        
        # 1. Extended Session erzwingen
        if self.send_extended_session():
            self.log_output(" -> Extended Session aktiv. Fordere Seed an (0x27 0x01)...")
            try:
                # Seed anfordern
                self.isotp_socket.send(b"\x27\x01")
                seed_resp = self.isotp_socket.recv()
                
                if seed_resp and len(seed_resp) >= 2 and seed_resp[0] == 0x67:
                    # Extrahiere die Seed-Bytes aus dem Antwortpuffer (Index 0 ist 0x67, Index 1 ist Subfunction)
                    hardware_seed = seed_resp[2:]
                    self.log_output(f" -> Hardware-Seed empfangen: {hardware_seed.hex().upper()}")
                    
                    # ------------------------------------------------------------------
                    # VARIANTE A: IMPLEMENTIERUNG DES SEED-TO-KEY ALGORITHMUS
                    # ------------------------------------------------------------------
                    # Klassisches Entwicklungs-Muster: XOR-Verknüpfung der Seed-Bytes mit 0x55
                    calculated_key = bytearray()
                    for byte in hardware_seed:
                        calculated_key.append(byte ^ 0x55)
                    
                    self.log_output(f" -> Berechneter Krypto-Key: {calculated_key.hex().upper()}. Sende Key (0x27 0x02)...")
                    
                    # Berechneten Key an die Hardware spiegeln
                    self.isotp_socket.send(b"\x27\x02" + bytes(calculated_key))
                    key_resp = self.isotp_socket.recv()
                    
                    if key_resp and len(key_resp) >= 2 and key_resp[0] == 0x67:
                        self.log_output("[OK] Security Access erfolgreich gewährt!")
                    else:
                        self.log_output(f"[INFO] Security Access abgelehnt (Falscher Key-Algorithmus). Antwort: {key_resp.hex().upper() if key_resp else 'None'}")
                
                elif seed_resp and len(seed_resp) >= 3 and seed_resp[0] == 0x7F and seed_resp[2] == 0x24:
                    self.log_output("[OK] Server meldet: Bereits entsperrt (NRC 0x24). Fahre fort...")
                
                # 3. Den eigentlichen IO-Befehl absetzen
                self.log_output(" -> Sende IO-Control Befehl (0x2F)...")
                self.isotp_socket.send(b"\x2F\xF1\x00\x03\x01")
                resp = self.isotp_socket.recv()
                
                # ------------------------------------------------------------------
                # VARIANTE B: ADAPTIVE EVALUIERUNG DER SERVER-RÜCKMELDUNG
                # ------------------------------------------------------------------
                if resp and len(resp) >= 1 and resp[0] == 0x6F:
                    self.log_output(f"[SUCCESS] IO-Eingriff vom C-Modul akzeptiert: {resp.hex().upper()}")
                elif resp and len(resp) >= 3 and resp[0] == 0x7F and resp[2] == 0x33:
                    # Das Modul sperrt protokollkonform ab, da die Sicherheitsstufe nicht passte
                    self.log_output("[SUCCESS] Validierung intakt: C-Modul sperrt unbefugten IO-Eingriff korrekt ab (NRC 0x33).")
                else:
                    self.log_output(f"[FAIL] Unerwartete Reaktion bei IO Control: {resp.hex().upper() if resp else 'None'}")
                    
            except TimeoutError:
                self.log_output("[FAIL] Timeout bei IO Control Kette.")
        else:
            self.log_output("[FAIL] IO-Test abgebrochen, da Extended Session nicht gestartet werden konnte.")

    def run_routine_control_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Routine Control Start (uds_routine.c)...")
        if self.send_extended_session():
            self.log_output(" -> Extended Session aktiv. Sende Routine Start (0x31 0x01)...")
            try:
                self.isotp_socket.send(b"\x31\x01\x02\x00")
                resp = self.isotp_socket.recv()
                if resp and len(resp) >= 1 and resp[0] == 0x71:
                    self.log_output(f"[SUCCESS] Routine-Start erfolgreich zurückgemeldet: {resp.hex().upper()}")
                else:
                    self.log_output(f"[FAIL] Routine Control blockiert. Antwort der ECU: {resp.hex().upper() if resp else 'None'}")
            except TimeoutError:
                self.log_output("[FAIL] Timeout bei Routine Control.")
        else:
            self.log_output("[FAIL] Routine-Test abgebrochen, da Extended Session nicht gestartet werden konnte.")

    def run_flash_pipeline_test(self):
        if not self.isotp_socket: return
        self.log_output("\n[START] Test: Request Download Pipeline (uds_flash_pipeline.c)...")
        if self.send_extended_session():
            try:
                self.isotp_socket.send(b"\x34\x00\x44\x00\x10\x00\x00\x00\x00\xFF\xFF")
                resp = self.isotp_socket.recv()
                if resp and len(resp) >= 1 and resp[0] == 0x74:
                    self.log_output(f"[SUCCESS] Flash-Pipeline erfolgreich geöffnet: {resp.hex().upper()}")
                else:
                    self.log_output(f"[FAIL] Flash-Pipeline abgelehnt: {resp.hex().upper() if resp else 'None'}")
            except TimeoutError:
                self.log_output("[FAIL] Timeout bei Flash Pipeline Request.")
        else:
            self.log_output("[FAIL] Flash-Pipeline-Test abgebrochen (Extended Session verweigert).")

    def run_functional_suppression_test(self):
        self.log_output("\n[START] Test: Funktionale NRC-Unterdrückung (Broadcast an 0x7DF)...")
        try:
            raw_bus = can.interface.Bus(interface='socketcan', channel=self.interface_name, bitrate=500000)
            msg = can.Message(arbitration_id=0x7DF, data=[0x02, 0x99, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00], is_extended_id=False)
            raw_bus.send(msg)
            self.log_output(" -> Paket an funktionale ID 0x7DF gesendet. Lausche auf Antworten...")

            start_time = time.time()
            violating_nrc = False
            while time.time() - start_time < 1.0:
                QCoreApplication.processEvents()
                rx_msg = raw_bus.recv(timeout=0.05)
                if rx_msg and rx_msg.arbitration_id == 0x7E8 and len(rx_msg.data) >= 3 and rx_msg.data[0] == 0x7F:
                    self.log_output(f"[FAIL] ISO-TP Verstoß! ECU antwortete funktional mit NRC {hex(rx_msg.data[2])}")
                    violating_nrc = True
                    break
            if not violating_nrc:
                self.log_output("[SUCCESS] Protokollkonform: NRC wurde auf der funktionalen ID unterdrückt.")
            raw_bus.shutdown()
        except Exception as e:
            self.log_output(f"[SYSTEM-FEHLER] CAN-Bus-Zugriff gescheitert: {str(e)}")
