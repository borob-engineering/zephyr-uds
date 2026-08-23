import isotp

class UDSEngine:
    """Kapselt die ISO-TP Socket-Verbindung und die UDS-Dienste nach ISO 14229-1."""
    def __init__(self, interface='can0', tx_id=0x7E0, rx_id=0x7E8):
        self.interface = interface
        self.tx_id = tx_id
        self.rx_id = rx_id
        self.socket = None

    def connect(self):
        """Initialisiert das ISO-TP Socket und bindet es an das Interface."""
        self.socket = isotp.socket()
        tp_address = isotp.Address(rxid=self.rx_id, txid=self.tx_id)
        self.socket.bind(self.interface, tp_address)

    def disconnect(self):
        """Schließt das ISO-TP Socket."""
        if self.socket:
            self.socket.close()

    def send_request(self, payload: list) -> bytes:
        """Sendet einen Request und fängt das asynchrone NRC 0x78 (Response Pending) ab."""
        if not self.socket:
            raise ConnectionError("ISO-TP Socket ist nicht verbunden.")
        
        self.socket.send(bytes(payload))
        
        while True:
            response = self.socket.recv()
            if not response:
                return b""
            
            # NRC 0x78 Handling (Response Pending): Weiter in der Schleife auf finale Antwort warten
            if len(response) >= 3 and response[0] == 0x7F and response[2] == 0x78:
                continue 
            
            return response

    def change_session(self, session_type: int) -> bytes:
        """Service 0x10: Diagnostic Session Control"""
        return self.send_request([0x10, session_type])

    def read_did(self, did: int) -> bytes:
        """Service 0x22: Read Data By Identifier"""
        return self.send_request([0x22, (did >> 8) & 0xFF, did & 0xFF])

    def write_did(self, did: int, data: list) -> bytes:
        """Service 0x2E: Write Data By Identifier"""
        return self.send_request([0x2E, (did >> 8) & 0xFF, did & 0xFF] + data)

    def request_seed(self, level=0x01) -> bytes:
        """Service 0x27: Security Access (Seed anfordern)"""
        return self.send_request([0x27, level])

    def send_key(self, level=0x02, key=None) -> bytes:
        """Service 0x27: Security Access (Key senden)"""
        if key is None:
            key = []
        return self.send_request([0x27, level] + key)