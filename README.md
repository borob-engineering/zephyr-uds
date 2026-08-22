# Funktionsübersicht: Generischer UDS-ISO-TP-Server für Zephyr RTOS

Das Modul fungiert als vollständig entkoppelte, hardwareunabhängige **Protokoll-Engine (Services & Timing)** nach ISO 14229-1 und ISO 15765-2. Alle anwendungsspezifischen Daten und Hardwarezugriffe werden über eine compilezeit-sichere `__weak`-Schnittstelle an die Applikationsschicht delegiert.

---

### 📥 1. Transport- & Netzwerkschicht (ISO 15765-2 / CAN-Bus)
*   **Dual-Binding-Empfangsschleife**: Gleichzeitiges, blockierungsfreies Verarbeiten von physikalischen (knotenspezifisch, z. B. `0x7E0`) und funktionalen (global im Netzwerk, z. B. `0x7DF`) CAN-IDs in einem dedizierten Worker-Thread.
*   **Protokollkonforme NRC-Unterdrückung**: Vollständige Unterdrückung von negativen Antworten (*Negative Response Codes* wie `ServiceNotSupported`) bei funktional adressierten Paketen nach ISO 14229-1, um Bus-Verstopfungen im Fahrzeugnetzwerk zu verhindern.
*   **ISO-TP Flusssteuerung (Flow Control)**: Automatisches Handling segmentierter Multi-Frame-Übertragungen (Consecutive Frames) inklusive dynamischer Aushandlung von Blockgröße (BS) und minimaler Trennzeit (STmin).

---

### ⏱️ 2. Integrierte Timing- & Schutzmechanismen
*   **S3-Verbindungstimer**: Automatische `k_timer`-Überwachung der Tester-Aktivität. Bleibt eine Anfrage (inkl. *Tester Present*) für mehr als 5000 ms aus, schaltet das Modul autark in die `DEFAULT_SESSION` zurück und sperrt alle Sicherheitsstufen.
*   **Asynchrones NRC 0x78 Handling (Response Pending)**: Integriert in zeitkritische Operationen wie *Clear DTC (0x14)* und *Routine Control (0x31)*. Der Core sendet sofort ein `0x7F SID 0x78`, um ein Tester-Timeout zu verhindern. Die finale positive Antwort wird erst abgesetzt, wenn der Hintergrund-Task in der Zephyr System-Workqueue (`k_work`) erfolgreich beendet wurde.
*   **Anti-Brute-Force-Sperre**: Automatischer, zeitbasierter Lockout (10.000 ms) via Kernel-Timer nach 3 aufeinanderfolgenden Fehlversuchen bei der Key-Eingabe im *Security Access*. Während der Sperre wird jede Anfrage mit **NRC 0x36** (*ExceededNumberOfAttempts*) abgewiesen.

---

### 🧩 3. Unterstützte UDS-Dienste (ISO 14229-1 Services)

| Service ID | Service Name | Implementierter Funktionsumfang & Spezifikationen |
| :--- | :--- | :--- |
| **0x10** | Diagnostic Session Control | Umschaltung und Zustandsverwaltung für `DEFAULT` (`0x01`), `PROGRAMMING` (`0x02`) und `EXTENDED` (`0x03`) Sessions. Setzt die Security bei Wechsel zurück. |
| **0x11** | ECU Reset | Hard- (`0x01`) und Soft-Reset (`0x03`) via Zephyrs native `sys_reboot` API. Asynchron verzögert, damit die positive Antwort den CAN-Transceiver sicher verlassen kann. |
| **0x14** | Clear Diagnostic Information | Asynchrones Löschen des Fehlerspeichers via `k_work`. Bietet eine interne Modul-API, um die Fehlerzustände in der DTC-Datenbank live zu nullen. |
| **0x19** | Read DTC Information | Sub-Function `0x02` (*reportDTCByStatusMask*) zum strukturierten Auslesen aktiver Fehlercodes inklusive standardkonformer Status-Verfügbarkeitsmaske. |
| **0x22** | Read Data By Identifier | Generisches Lesen von DIDs. Formatiert die positive Antwort (`SID + 0x40`) und bettet die über das Applikations-Interface angeforderten Daten ein. |
| **0x27** | Security Access | Seed/Key-Verfahren (Level 1) unter Verwendung des echten Hardware-Zufallsgenerators (**Zephyr Entropy Driver**) für kryptografisch sichere Seeds. Blockiert funktionale Seed-Anfragen (**NRC 0x22**). |
| **0x2E** | Write Data By Identifier | Generisches Schreiben von DIDs. Beinhaltet eine integrierte Session-Validierung (Schreibblockade außerhalb der `EXTENDED` Session via **NRC 0x7E** *SubFunctionNotSupportedInActiveSession*). |
| **0x2F** | Input Output Control By Identifier | Protokoll-Wrapper für den Stellgliedtest. Unterstützt `shortTermAdjustment` (`0x03`) zur manuellen Übernahme und `returnControlToECU` (`0x00`) zur Freigabe von ECU-Zuständen. |
| **0x31** | Routine Control | Starten (`0x01`) und Abfragen von Ergebnissen (`0x03`) asynchroner Hintergrundroutinen (z. B. Speicherlöschung vor dem Flashen) via `k_work` und **NRC 0x78**. |
| **0x34** | Request Download | Einleitung der Software-Flashing-Pipeline. Validiert Datenlängen/Adressformate und signalisiert dem Tester die maximal zulässige ISO-TP Blockgröße (`UDS_BUFF_SIZE`). |
| **0x36** | Transfer Data | Streaming-Pipeline für Firmware-Binärdaten. Überwacht und erzwingt die strikte Einhaltung des sequenziellen `blockSequenceCounter` zur Vermeidung von Datenverlust (**NRC 0x73**). |
| **0x37** | Request Transfer Exit | Schließen der Flash-Datenübertragung und Rücksetzung der Streaming-Pipeline in den Ruhezustand (*FLASH_STATE_IDLE*). |
| **0x3E** | Tester Present | Verbindungsaufrechterhaltung. Unterstützt die native Unterdrückung der positiven Antwort über das *SuppressPositiveResponse* Bit (Bit 7 des Sub-Functions). |

---

### 🔌 4. Abstraktionsebene (Application Interface)
Der Treiber deklariert folgende Funktionen als `__weak` Fallbacks mit vordefinierten Fehlermustern (z. B. **NRC 0x31** *RequestOutOfRange*), welche von der Applikation ohne Registrierungsaufwand überschrieben werden können:
*   `uds_app_read_did()`: Übergibt gelesene Rohdaten an den CAN-Buffer.
*   `uds_app_write_did()`: Übergibt empfangene Datenblöcke zur Persistierung (z. B. NVS/Flash).
*   `uds_app_calculate_key()`: Schnittstelle für den OEM-spezifischen Seed-to-Key Krypto-Algorithmus.
*   `uds_app_io_control()`: Übergibt Stellgliedbefehle an die Hardware (z. B. Zephyr GPIO-Treiber).
*   `uds_app_routine_start()` / `uds_app_routine_request_results()`: Startet und überwacht anwendungsspezifische System-Tasks.

---

## 🖥️ 5. Integrierte GUI-Hardware-Testsuite

Für die Validierung der Protokoll-Engine gegen echte Hardware befindet sich im Projekt eine interaktive Python-Testsuite auf Basis von **PyQt5**, **python-can** und **can-isotp**. Diese erlaubt es, alle implementierten ISO 14229-1 Dienste (inklusive automatisierter Seed/Key-Berechnung und vollständigem Firmware-Streaming) per Knopfdruck an einem realen CAN-Bus zu testen.

### 🔌 Hardware-Inbetriebnahme (z. B. USB-zu-CAN-Umsetzer an /dev/ttyACM2)

Wenn ein USB-zu-CAN-Wandler mit slcan-Firmware verwendet wird, muss dieser vor dem Start des Skripts mit einer Baudrate von **500 kBit/s** in den Linux-Netzwerk-Stack eingebunden werden:

```bash
# 1. System-Werkzeuge für CAN installieren & Treiber laden
sudo apt update && sudo apt install -y can-utils
sudo modprobe can can_raw slcan

# 2. Seriellen Wandler an das SocketCAN-Interface 'slcan0' binden
sudo slcand -o -c -s6 /dev/ttyACM2 slcan0

# 3. Das Netzwerkinterface im System hochfahren
sudo ip link set slcan0 up
```

### 📦 Python-Abhängigkeiten installieren

Installiere die für die grafische Oberfläche und die Netzwerschicht erforderlichen Python-Pakete (ggf. mit `--break-system-packages` ergänzen, falls deine Linux-Distribution dies vorgibt):

```bash
pip install python-can can-isotp PyQt5
```

### 🏃‍♂️ Testsuite ausführen

Speichere das bereitgestellte Skript als `uds_hardware_tester.py` ab. Stelle sicher, dass im Initialisierungs-Block der Schnittstellenname exakt als String übergeben wird:
```python
interface_name = 'slcan0'
can_bus = can.interface.Bus(interface='socketcan', channel=interface_name, bitrate=500000)
isotp_socket = isotp.socket()
isotp_socket.bind(interface_name, tp_address)
```

Starte das grafische Interface anschließend über das Terminal (nutze `sudo`, falls dein Linux-Benutzer standardmäßig keine Berechtigungen für direkte Netzwerk-Sockets besitzt):

```bash
python uds_hardware_tester.py
# Alternativ: sudo \$(which python) uds_hardware_tester.py
```

Das Skript verwendet standardmäßig die physikalische UDS-Anfrage-ID **`0x7E0`** (TX) und horcht auf der ID **`0x7E8`** (RX) auf Antworten des Zephyr-Knotens.
