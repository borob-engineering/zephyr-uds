#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import threading
import time
import can

class UdsWorker(threading.Thread):
    def __init__(self, interface='socketcan', channel='can0', bitrate=500000, engine=None):
        """
        Worker-Thread zur blockierungsfreien CAN/ISO-TP-Datenverarbeitung.
        """
        super().__init__()
        self.interface = interface
        self.channel = channel
        self.bitrate = bitrate
        self.engine = engine
        self.running = True
        self.bus = None

        try:
            # Verbindung zum CAN-Interface herstellen
            self.bus = can.interface.Bus(interface=self.interface, channel=self.channel, bitrate=self.bitrate)
        except Exception as e:
            print(f"[Worker Error] CAN-Bus-Initialisierung fehlgeschlagen: {e}")

    def run(self):
        print(f"[Worker] Höre auf USB-Interface '{self.channel}'...")
        while self.running:
            if self.bus is None:
                time.sleep(0.1)
                continue

            try:
                msg = self.bus.recv(timeout=0.1)
                if msg is None:
                    continue

                # Wenn die ECU antwortet (0x7E8 = Steuergeräte-Antwort ID)
                if msg.arbitration_id == 0x7E8:
                    # Konvertiere die CAN-Rohdaten zurück in ein kompaktes bytes-Objekt
                    rx_bytes = bytes(msg.data)
                    
                    # Wenn eine GUI an den Worker gekoppelt ist, triggere deren Logger
                    # (Wir prüfen dynamisch, ob die Applikation läuft)
                    if hasattr(self, 'gui_instance') and self.gui_instance:
                        self.gui_instance.gui_receive_logger(rx_bytes)
                    else:
                        # Fallback: Falls keine GUI offen ist, verarbeite es in der Engine
                        if self.engine:
                            self.engine.process_rx_frame(rx_bytes)

            except Exception as e:
                print(f"[Worker Exception] CAN-Fehler: {e}")


    def stop(self):
        """Stoppt die Netzwerkschleife sauber."""
        self.running = False
        if self.bus:
            try:
                self.bus.shutdown()
            except:
                pass
        print("[Worker] Thread gestoppt.")
