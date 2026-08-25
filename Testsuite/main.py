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
    # Liest den String sauber über sys.argv[1] aus, um Listen-Fehler zu vermeiden
    if len(sys.argv) > 1:
        can_channel = sys.argv[1]
    else:
        can_channel = "can0"
        
    print(f"[UDS-Tester] Hardware-Modus aktiv auf Interface: '{can_channel}'")
    
    # 2. Native Linux ISO-TP Socket konfigurieren
    socket = isotp.socket(timeout=1.0)
    try:
        # Feste 8-Byte DLC-Längen zwingend für Zephyr RTOS vorgeben
        socket.set_opts(txpad=0xAA, rxpad=0xAA)
        
        # Tuning: Sende alle Consecutive Frames ohne künstliche Bus-Pausen (BS=0, STmin=0)
        socket.set_fc_opts(bs=0, stmin=0)
        
        # rxid = 0x7E8 (ECU sendet), txid = 0x7E0 (Tester sendet)
        addr = isotp.Address(rxid=0x7E8, txid=0x7E0)
        socket.bind(can_channel, addr)
        
        # Socket auf nicht-blockierend setzen, um den asynchronen RX-Thread zu füttern
        if hasattr(socket, '_socket'):
            socket._socket.setblocking(False)
    except Exception as e:
        print(f"[Socket Fatal] ISO-TP Bindung fehlgeschlagen: {e}")
        print("Hinweis: Ist das Interface aktiv? (z.B. sudo ip link set up can0)")
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

    # 4. GUI erzeugen und den UDS-Client sowie das Socket übergeben
    gui = UdsGui(uds_client=uds_client, socket=socket)
    
    # ==============================================================================
    # ASYNCHRONER EMPFANGS-THREAD (Mit sauberer Exit-Kontrolle)
    # ==============================================================================
    # Kontroll-Flag für den Thread-Lebenszyklus zur Vermeidung von Shutdown-Fehlern
    thread_control = {'running': True}

    def rx_thread_loop():
        while thread_control['running']:
            try:
                # Versuche Rohdaten aus dem Socket zu lesen, um den Puffer zu leeren
                rx_data = socket.recv()
                if rx_data:
                    # Signalisiere der GUI thread-sicher, dass Daten eingetroffen sind
                    gui.trigger_rx_signal(bytes(rx_data))
            except (BlockingIOError, OSError):
                time.sleep(0.005)
            except Exception as e:
                # Fehlermeldungen während des Shutdowns unterdrücken
                if thread_control['running']:
                    print(f"[Rx Thread Error] {e}")
                time.sleep(0.01)

    rx_thread = threading.Thread(target=rx_thread_loop)
    rx_thread.daemon = True
    rx_thread.start()

    # GUI anzeigen und Qt-App-Schleife starten
    gui.show()
    exit_code = app.exec_()
    
    # ==============================================================================
    # RESSOURCEN BEIM SCHLIESSEN SAUBER FREIGEBEN (3-STUFEN-SHUTDOWN)
    # ==============================================================================
    # Schritt 1: Hintergrund-Thread stoppen
    thread_control['running'] = False
    time.sleep(0.02) # Kurze Pause, damit der Thread die Schleife sauber verlässt
    
    # Schritt 2: UDS Client schließen (und den internen rxthread-Bug abfangen)
    try:
        uds_client.close()
    except AttributeError:
        pass
    except Exception as e:
        print(f"[Shutdown Info] Fehler beim Client-Shutdown: {e}")
    
    # Schritt 3: Erst jetzt das physische OS-Socket schließen
    try:
        socket.close()
        print("[System] ISO-TP Hardware-Interface erfolgreich geschlossen.")
    except Exception as e:
        print(f"[Shutdown Info] Fehler beim Socket-Shutdown: {e}")
        
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
