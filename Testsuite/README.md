# 🖥 UDS-ISO-TP Hardware Testsuite

Diese intuitive, rein hardwarebasierte Python-Testsuite dient der vollständigen Protokoll- und Timing-Validierung des **Zephyr RTOS UDS-Servers** nach den Normen **ISO 14229-1 (UDS)** und **ISO 15765-2 (ISO-TP)**. 

Das System arbeitet zu 100 % asynchron über die native SocketCAN-Schnittstelle des Linux-Kernels und verzichtet vollständig auf künstliche RAM-Simulationen. Dadurch werden Multi-Frame-Übertragungen (z. B. DTC-Listen) und Timing-kritische Abläufe präzise und hardwarenah getestet.

---

## 🛠 Architektur & Komponenten

Das Tool ist modular aufgebaut und besteht aus zwei zentralen Dateien:

1. **`main.py` (Hardware-Starter & Netzwerk-Core)**
   * Bindet und konfiguriert das native `can-isotp` Socket des Linux-Kernels.
   * Erzwingt festes Tx/Rx-Padding (`0xAA`) für konforme 8-Byte DLC-Längen, um Filter-Fehler im Zephyr-Knoten zu vermeiden.
   * Setzt Flusssteuerungs-Parameter (`bs=0`, `stmin=0`), um Multi-Frame-Timeouts bei großen Datenmengen zu verhindern.
   * Betreibt einen autarken, nicht-blockierenden Empgfangsthread (`BlockingIOError`-geschützt) für 0% CPU-Last im Ruhezustand.

2. **`uds_gui.py` (PyQt5 Bedienoberfläche & Protokoll-Übersetzer)**
   * Visuelle Steuerung aller implementierten ISO 14229-1 Dienste per Knopfdruck.
   * **Live-Übersetzer (DBC-unabhängig):** Decodiert ausgehende Hex-Requests (TX) und eintreffende Steuergeräte-Antworten (RX) sowie Fehlercodes (NRCs) direkt im Logfenster in verständlichen Klartext.
   * Integrierte automatisierte Abläufe: Seed/Key-Krypto-Handshake (XOR 0xFF) und blockweise segmentiertes Datei-Streaming (256-Byte Chunks) für die Flash-Pipeline.
   * Rechtsbündiger Button zum sofortigen Leeren des Log-Verlaufs.

---

## 🔌 Unterstützte UDS-Dienste (ISO 14229-1)

| Service ID | Dienstname | Validierter Funktionsumfang |
| :--- | :--- | :--- |
| **0x10** | Diagnostic Session Control | Umschaltung zwischen Default (0x01), Programming (0x02) und Extended (0x03). |
| **0x11** | ECU Reset | Sendet Hard- oder Soft-Reset; prüft, ob die ECU die Antwort vor dem Reboot absetzt. |
| **0x14** | Clear Diagnostic Info | Signalisiert das Löschen des gesamten Fehlerspeichers (Gruppe 0xFFFFFF). |
| **0x19** | Read DTC Information | Fordert die aktive Fehlerliste an (`reportDTCByStatusMask`). Multi-Frame erprobt. |
| **0x22** | Read Data By Identifier | Liest vordefinierte DIDs aus dem Speicher des Steuergeräts aus (z. B. VIN `0xF190`). |
| **0x27** | Security Access | Initiiert den Seed-Request und schickt den berechneten Key automatisiert hinterher. |
| **0x2E** | Write Data By Identifier | Beschreibt DIDs (inkl. Überwachung der Sitzungs-Sperre außerhalb von Extended). |
| **0x2F** | Input Output Control | Simuliert Stellgliedtests (z. B. LEDs via `ShortTermAdjustment` / `ReturnControl`). |
| **0x31** | Routine Control | Startet oder stoppt asynchrone Hintergrundroutinen der Hardware (z. B. Erase vor Flash). |
| **0x34** | Request Download | Initialisiert die Flash-Pipeline und verhandelt die Blocklängen für das Firmware-Update. |
| **0x36** | Transfer Data | Streamt die geladene `.bin`-Datei blockweise mit fortlaufendem Sequenzzähler an die ECU. |
| **0x37** | Request Transfer Exit | Schließt die Flash-Pipeline und versetzt den Flash-Treiber wieder in den Ruhezustand. |

---

## 📦 System-Voraussetzungen

Stelle sicher, dass dein Linux-System mit den notwendigen Kernel-Modulen und Python-Paketen ausgestattet ist:

```bash
# 1. Installiere die Python-Abhängigkeiten im Workspace
pip3 install PyQt5 can-isotp

# 2. Lade das native Linux-ISO-TP-Kernelmodul
sudo modprobe can-isotp
```

---

## 🏃‍♂️ Inbetriebnahme & Anwendung

### 1. CAN-Interface konfigurieren
Initialisiere dein physikalisches USB-zu-CAN-Interface (oder einen virtuellen Bus für lokale Integrationstests) mit der exakten Bitrate des Zephyr-Steuergeräts (Standard: 500 kBit/s):

```bash
# Für echte Hardware-Adapter (z.B. Peak-CAN, Candlelight):
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up

# Alternativ für rein virtuelle Vortests (vcan):
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set up vcan0
```

### 2. Testsuite starten
Das Skript wertet beim Start das erste Kommandozeilen-Argument aus. Wird kein Interface angegeben, nutzt das Tool automatisch das Fallback **`can0`**.

```bash
# Startet standardmäßig auf physikalischem 'can0'
python3 main.py

# Startet explizit auf einem virtuellen Interface
python3 main.py vcan0

# Startet auf einem sekundären Hardware-Kanal
python3 main.py can1
```

### 3. Log-Überwachung nutzen
Jede Interaktion wird im Überwachungsfenster farblos, strukturiert und mit direkter Protokoll-Erklärung ausgegeben. Sollte das Zephyr-Board eine Anfrage verwerfen, wird der genaue **Negative Response Code (NRC)** direkt im Klartext aufgeschlüsselt (z. B. `IncorrectMessageLengthOrInvalidFormat`), was die Fehlersuche drastisch verkürzt.
