#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import threading
import isotp
from PyQt5.QtWidgets import QApplication
from uds_engine import UdsEngine
from uds_gui import UdsGui

def main():
    app = QApplication(sys.argv)
    
    # 1. Protokoll-Engine instanziieren
    engine = UdsEngine()
    
    # 2. CAN-Device aus den Argumenten auslesen (Standard: 'can0')
    if len(sys.argv) > 1:
        can_channel = sys.argv[1]  # <--- KORREKTUR HIER
    else:
        can_channel = "can0"
        
    print(f"[Hardware-Start] Aktiviere ISO-TP Socket auf: '{can_channel}'")
    
    # 3. GUI erzeugen
    gui = UdsGui(worker=None, engine=engine)
    
    # 4. Echtes ISO-TP Socket konfigurieren und öffnen
    socket = isotp.socket(timeout=2.0)
    
    try:
        socket.set_opts(txpad=0xAA, rxpad=0xAA)
        socket.bind(can_channel, isotp.Address(rxid=0x7E8, txid=0x7E0))
    except Exception as e:
        print(f"[Socket Error] Konnte ISO-TP Socket auf {can_channel} nicht öffnen: {e}")
        print("Hinweis: Stelle sicher, dass das Interface aktiv ist (z.B. sudo ip link set up can0)")
        sys.exit(1)

    # ==============================================================================
    # HARDWARE-BRÜCKE: GUI-Klicks über das ISO-TP Socket senden
    # ==============================================================================
    def hardware_isotp_send(payload):
        """Sendet UDS-Payloads normkonform über die ISO-TP Schicht."""
        try:
            socket.send(payload)
            gui.log(f"[TX ISO-TP] {payload.hex().upper()}")
        except Exception as e:
            gui.log(f"[ISO-TP Fehler] Senden fehlgeschlagen: {e}")

    # Die Simulations-Sende-Methode der GUI mit der echten ISO-TP-Schnittstelle verknüpfen
    gui.send_raw_request = hardware_isotp_send

    # ==============================================================================
    # EMPFANGS-THREAD: Höre auf Antworten des Zephyr-Steuergeräts
    # ==============================================================================
    def rx_thread_loop():
        while True:
            try:
                rx_data = socket.recv()
                if rx_data:
                    gui.gui_receive_logger(bytes(rx_data))
            except Exception as e:
                time.sleep(0.01)

    rx_thread = threading.Thread(target=rx_thread_loop)
    rx_thread.daemon = True
    rx_thread.start()

    # 5. GUI anzeigen und App-Schleife ausführen
    gui.show()
    exit_code = app.exec_()
    
    socket.close()
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
