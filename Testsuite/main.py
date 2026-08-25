#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import time
import threading
import isotp
from PyQt5.QtWidgets import QApplication
from uds_gui import UdsGui

def main():
    # Initialisiere das PyQt5 Anwendungs-Framework
    app = QApplication(sys.argv)
    
    # 1. CAN-Device aus den Argumenten auslesen (Standard: 'can0')
    # Erlaubt den Aufruf via 'python3 main.py vcan0' oder 'python3 main.py can1'
    if len(sys.argv) > 1:
        can_channel = sys.argv[1]
    else:
        can_channel = "can0"
        
    print(f"[UDS-Tester] Dedizierter Hardware-Modus aktiv auf Interface: '{can_channel}'")
    
    # 2. Erzeuge das native ISO-TP Socket
    # Das Timeout von 1.0 Sekunde sichert ab, dass das Socket bei Verbindungsabbrüchen nicht einfriert
    socket = isotp.socket(timeout=1.0)
    
    try:
        # txpad und rxpad zwingen das Linux-Kernelmodul auf feste 8-Byte DLC-Längen.
        # Das ist zwingend erforderlich, um DLC-Validierungsfehler im Zephyr RTOS zu verhindern.
        socket.set_opts(txpad=0xAA, rxpad=0xAA)
        
        # PROTOKOLL-WORKAROUND FÜR MULTI-FRAME-TIMEOUTS:
        # bs=0    -> Block Size 0 (Teilt der ECU mit: "Sende alle Consecutive Frames ohne Unterbrechung")
        # stmin=0 -> Separation Time 0 (Fordert minimale Trennzeit von 0ms zwischen den Rahmen an)
        socket.set_fc_opts(bs=0, stmin=0)
        
        # KORREKTE UDS-ADRESSIERUNG NACH ISO 15765-2:
        # rxid: Die ID, auf der der PC (Python) Antworten der ECU erwartet -> 0x7E8
        # txid: Die ID, auf der der PC (Python) Anfragen an die ECU sendet -> 0x7E0
        addr = isotp.Address(rxid=0x7E8, txid=0x7E0)
        
        # Binde das Socket an das gewählte CAN-Interface
        socket.bind(can_channel, addr)
        print("[System] ISO-TP Socket erfolgreich konfiguriert und an Kernel gebunden.")
    except Exception as e:
        print(f"[Socket Fatal] Bindung oder Konfiguration fehlgeschlagen: {e}")
        print("Hinweis: Stelle sicher, dass das CAN-Interface aktiv ist (z.B. sudo ip link set up can0)")
        sys.exit(1)

    # 3. GUI erzeugen und das scharfgeschaltete Socket übergeben
    # Da wir rein hardwarebasiert arbeiten, entfällt der Simulations-Parameter (Engine) vollständig.
    gui = UdsGui(socket=socket)
    
    # ==============================================================================
    # ASYNCHRONER NETZWERK-EMPFANGSTHREAD
    # ==============================================================================
    def rx_thread_loop():
        # Schalte den nicht-blockierenden Modus über das native OS-Socket scharf
        if hasattr(socket, '_socket'):
            socket._socket.setblocking(False)
        
        while True:
            try:
                # Versuche, ein vollständig zusammengesetztes UDS-Paket aus dem Kernel-Puffer zu lesen
                rx_data = socket.recv()
                if rx_data:
                    # Leite das fertige Multi-Frame-Paket an den Klartext-Logger der GUI weiter
                    gui.gui_receive_logger(bytes(rx_data))
            except (BlockingIOError, OSError):
                # KORREKTUR: Fängt Errno 11 (Resource temporarily unavailable) sauber ab.
                # Keine Daten im Puffer vorhanden -> Kurz schlafen, um CPU-Last auf 0% zu halten.
                time.sleep(0.005)
            except Exception as e:
                print(f"[Rx Thread Error] {e}")
                time.sleep(0.01)


    # Starte den Empfangsthread als Daemon, damit er beim Schließen der GUI automatisch beendet wird
    rx_thread = threading.Thread(target=rx_thread_loop)
    rx_thread.daemon = True
    rx_thread.start()

    # 4. GUI-Fenster anzeigen und die Qt-Ereignisschleife starten
    gui.show()
    exit_code = app.exec_()
    
    # Ressourcen beim Schließen der Anwendung sauber freigeben
    socket.close()
    sys.exit(exit_code)

if __name__ == "__main__":
    main()
