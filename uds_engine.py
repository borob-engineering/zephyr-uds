#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import struct

class UdsEngine:
    def __init__(self, send_callback=None):
        """
        Initialisiert die UDS-Protokoll-Engine.
        :param send_callback: Funktion, die aufgerufen wird, um Antworten an den Bus zu senden.
        """
        self.send_callback = send_callback
        self.current_session = 0x01          # 0x01 = DEFAULT, 0x02 = PROGRAMMING, 0x03 = EXTENDED
        self.security_level = 0x00           # 0x00 = Locked / Keine Security
        self.flash_state = 0x00              # 0 = IDLE, 1 = DOWNLOAD_REQUESTED, 2 = TRANSFERRING
        self.block_sequence_counter = 1      # Zähler für Service 0x36 (Transfer Data)
        self.generated_seed = b""

    def process_rx_frame(self, rx_data):
        """
        Verarbeitet ein empfangenes ISO-TP/UDS-Paket und triggert den Callback.
        """
        if not rx_data or len(rx_data) == 0:
            return

        sid = rx_data[0]

        # --- 0x10: Diagnostic Session Control ---
        if sid == 0x10:
            if len(rx_data) < 2:
                self._send_nrc(0x10, 0x13) # IncorrectMessageLengthOrInvalidFormat
                return
            sub_func = rx_data[1] & 0x7F # SuppressPositiveResponse-Bit ausmaskieren
            if sub_func in [0x01, 0x02, 0x03]:
                self.current_session = sub_func
                self.security_level = 0x00 # Zurücksetzen der Security bei Session-Wechsel
                self.send_callback(bytes([0x50, sub_func]))
            else:
                self._send_nrc(0x10, 0x12) # SubFunctionNotSupported

        # --- 0x11: ECU Reset ---
        elif sid == 0x11:
            if len(rx_data) < 2:
                self._send_nrc(0x11, 0x13)
                return
            sub_func = rx_data[1] & 0x7F
            if sub_func in [0x01, 0x03]: # Hard- oder Softreset
                # Sende erst positive Antwort (0x51), bevor die simulierte Hardware neu startet
                self.send_callback(bytes([0x51, sub_func]))
                self.current_session = 0x01
                self.security_level = 0x00
                self.flash_state = 0x00
            else:
                self._send_nrc(0x11, 0x12)

        # --- 0x14: Clear Diagnostic Information ---
        elif sid == 0x14:
            if len(rx_data) < 4:
                self._send_nrc(0x14, 0x13)
                return
            # Bestätigung senden (0x14 + 0x40 = 0x54)
            self.send_callback(bytes([0x54]))

        # --- 0x19: Read DTC Information ---
        elif sid == 0x19:
            if len(rx_data) < 2:
                self._send_nrc(0x19, 0x13)
                return
            sub_func = rx_data[1] & 0x7F
            if sub_func == 0x02: # reportDTCByStatusMask
                # Simulierter aktiver Fehlercode: DTC 0x123456 mit Status 0x01 (aktiv)
                # Format: SID+0x40, Subfunc, AvailabilityMask (0x09), DTC-Bytes...
                response = bytes([0x59, 0x02, 0x09, 0x12, 0x34, 0x56, 0x01])
                self.send_callback(response)
            else:
                self._send_nrc(0x19, 0x12)

        # --- 0x22: Read Data By Identifier ---
        elif sid == 0x22:
            if len(rx_data) < 3:
                self._send_nrc(0x22, 0x13)
                return
            did = struct.unpack(">H", rx_data[1:3])[0]
            if did == 0xF190: # Beispiel-DID: VIN
                vin_data = b"ZEPHYR-UDS-SIM001"
                self.send_callback(bytes([0x62]) + rx_data[1:3] + vin_data)
            else:
                self._send_nrc(0x22, 0x31) # RequestOutOfRange

        # --- 0x27: Security Access ---
        elif sid == 0x27:
            if len(rx_data) < 2:
                self._send_nrc(0x27, 0x13)
                return
            sub_func = rx_data[1]
            
            # Request Seed (Ungerade Subfunktionen)
            if sub_func == 0x01:
                self.generated_seed = b"\xAB\xCD\xEF\x01" # Simulierter Hardware-Zufallwert
                self.send_callback(bytes([0x67, 0x01]) + self.generated_seed)
            
            # Send Key (Gerade Subfunktionen)
            elif sub_func == 0x02:
                if not self.generated_seed:
                    self._send_nrc(0x27, 0x24) # RequestSequenceError
                    return
                provided_key = rx_data[2:]
                # Einfacher Beispiel-Kryptoalgorithmus: Key = Seed XOR 0xFF
                expected_key = bytes([b ^ 0xFF for b in self.generated_seed])
                if provided_key == expected_key:
                    self.security_level = 0x01
                    self.send_callback(bytes([0x67, 0x02]))
                else:
                    self._send_nrc(0x27, 0x35) # InvalidKey
            else:
                self._send_nrc(0x27, 0x12)

        # --- 0x2E: Write Data By Identifier ---
        elif sid == 0x2E:
            if len(rx_data) < 4:
                self._send_nrc(0x2E, 0x13)
                return
            if self.current_session != 0x03: # Schreibblockade außerhalb der EXTENDED Session
                self._send_nrc(0x2E, 0x7E) # SubFunctionNotSupportedInActiveSession
                return
            # Positive Rückmeldung: 0x6E + DID
            self.send_callback(bytes([0x6E]) + rx_data[1:3])

        # --- 0x2F: Input Output Control By Identifier ---
        elif sid == 0x2F:
            if len(rx_data) < 4:
                self._send_nrc(0x2F, 0x13)
                return
            # Positive Response spiegelt DID und Control-Parameter (0x00=ReturnControl, 0x03=ShortTermAdjustment)
            response = bytes([0x6F]) + rx_data[1:4]
            self.send_callback(response)

        # --- 0x31: Routine Control ---
        elif sid == 0x31:
            if len(rx_data) < 4:
                self._send_nrc(0x31, 0x13)
                return
            sub_func = rx_data[1] # 0x01=Start, 0x03=RequestResults
            # Positive Response: 0x71 + SubFunc + RoutineID
            response = bytes([0x71, sub_func]) + rx_data[2:4]
            self.send_callback(response)

        # --- 0x34: Request Download ---
        elif sid == 0x34:
            if self.current_session != 0x02: # Muss zwingend in PROGRAMMING sein
                self._send_nrc(0x34, 0x7E)
                return
            self.flash_state = 1 # Zustand: Download freigegeben
            self.block_sequence_counter = 1
            # Antwort: 0x74 + MaxNumberOfBlockLength (0x02 0x00 -> Max 512 Bytes pro Block)
            self.send_callback(bytes([0x74, 0x20, 0x02, 0x00]))

        # --- 0x36: Transfer Data ---
        elif sid == 0x36:
            if self.flash_state not in [1, 2]:
                self._send_nrc(0x36, 0x24) # RequestSequenceError
                return
            bsc = rx_data[1] if len(rx_data) > 1 else 0
            if bsc != self.block_sequence_counter:
                self._send_nrc(0x36, 0x73) # WrongBlockSequenceCounter
                return
            
            self.flash_state = 2 # Zustand: Datenübertragung läuft aktiv
            # Erhöhe rollierend im Wertebereich 1-255
            self.block_sequence_counter = (self.block_sequence_counter + 1) if self.block_sequence_counter < 255 else 1
            # Antwort: 0x76 + aktueller BlockSequenceCounter
            self.send_callback(bytes([0x76, bsc]))

        # --- 0x37: Request Transfer Exit ---
        elif sid == 0x37:
            if self.flash_state != 2:
                self._send_nrc(0x37, 0x24) # Sequence Error
                return
            self.flash_state = 0 # Zurück in IDLE-Zustand
            self.send_callback(bytes([0x77]))

        # --- Unerwarteter Dienst ---
        else:
            self._send_nrc(sid, 0x11) # ServiceNotSupported

    def _send_nrc(self, sid, nrc_code):
        """Hilfsfunktion zum Senden einer negativen Antwort."""
        if self.send_callback:
            self.send_callback(bytes([0x7F, sid, nrc_code]))
