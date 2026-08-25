#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import threading
import isotp
from udsoncan.client import Client
from udsoncan.connections import IsoTPSocketConnection
from PyQt5.QtWidgets import QApplication
from uds_gui import UdsGui

def main():
    app = QApplication(sys.argv)
    
    # 1. CAN-Device aus den Argumenten auslesen (Standard: 'can0')
    if len(sys.argv) > 1:
        can_channel = sys.argv[1]
    else:
        can_channel = "can0"
        
    print(f"[UDS-Tester] Hardware-Modus aktiv auf Interface: '{can_channel}'")
    
    # 2. Native Linux ISO-TP Socket konfigurieren
    socket = isotp.socket(timeout=1.0)
    try:
        # Feste 8-Byte DLC-Längen für Zephyr erzwingen
        socket.set_opts(txpad=0xAA, rxpad=0xAA)
        socket.set_fc_opts(bs=0, stmin=0)
        
        # rxid = 0x7E8 (ECU sendet), txid = 0x7E0 (Tester sendet)
        addr = isotp.Address(rxid=0x7E8, txid=0x7E0)
        socket.bind(can_channel, addr)
        
        # WICHTIG: Socket auf nicht-blockierend setzen, um Timeouts beim schnellen Wechsel zu verhindern
        if hasattr(socket, '_socket'):
            socket._socket.setblocking(False)
    except Exception as e:
        print(f"[Socket Fatal] ISO-TP Bindung fehlgeschlagen: {e}")
        sys.exit(1)

    # 3. Übergabe des Sockets via tpsock an die IsoTPSocketConnection
    connection = IsoTPSocketConnection(interface=can_channel, address=addr, tpsock=socket)
    
    # Großzügige Hardware-Timeouts definieren
    client_config = {
        'request_timeout': 3.0,
        'p2_timeout': 1.0,
        'p2_star_timeout': 3.0
    }
    
    # UDS Client instanziieren und öffnen
    uds_client = Client(connection, config=client_config)
    uds_client.open()

    # 4. GUI erzeugen und den UDS-Client übergeben
    gui = UdsGui(uds_client=uds_client, socket=socket)
    
    # ==============================================================================
    # ASYNCHRONER EMPFANGS-THREAD (Sichert den schnellen Empfang von Zephyr-Antworten)
    # ==============================================================================
    def rx_thread_loop():
        while True:
            try:
                # Versuche Rohdaten aus dem Socket zu lesen, um den Puffer zu leeren
                rx_data = socket.recv()
                if rx_data:
                    # Signalisiere der GUI thread-sicher, dass Daten eingetroffen sind
                    gui.trigger_rx_signal(bytes(rx_data))
            except (BlockingIOError, OSError):
                time.sleep(0.005)
            except Exception as e:
                print(f"[Rx Thread Error] {e}")
                time.sleep(0.01)

    rx_thread = threading.Thread(target=rx_thread_loop)
    rx_thread.daemon = True
    rx_thread.start()

    gui.show()
    exit_code = app.exec_()
    
    # Ressourcen sauber schließen
    uds_client.close()
    socket.close()
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
