import socket
import threading
import time
import logging

from server.config import DISCOVERY_PORT, SERVER_PORT, ENABLE_DISCOVERY

logger = logging.getLogger("switch-ocr.discovery")

class DiscoveryServer:
    def __init__(self, port: int = DISCOVERY_PORT, server_port: int = SERVER_PORT):
        self.port = port
        self.server_port = server_port
        self.running = False
        self.broadcast_thread = None
        self.listener_thread = None

    def _broadcast_loop(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        sock.settimeout(1.0)
        message = f"SWITCH_OCR_SERVER:{self.server_port}".encode("utf-8")

        while self.running:
            try:
                sock.sendto(message, ("<broadcast>", self.port))
            except Exception as e:
                logger.debug(f"UDP broadcast tick failed: {e}")
            time.sleep(2.0)
        sock.close()

    def _listener_loop(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sock.bind(("0.0.0.0", self.port))
            sock.settimeout(1.0)
        except Exception as e:
            logger.warning(f"Could not bind discovery listener on port {self.port}: {e}")
            return

        response = f"SWITCH_OCR_SERVER:{self.server_port}".encode("utf-8")
        while self.running:
            try:
                data, addr = sock.recvfrom(1024)
                text = data.decode("utf-8", errors="ignore").strip()
                if "DISCOVER" in text or "SWITCH_OCR" in text:
                    sock.sendto(response, addr)
                    time.sleep(0.03)
                    sock.sendto(response, addr)
                    logger.info(f"Answered discovery ping from Switch at {addr[0]}:{addr[1]}")
            except socket.timeout:
                continue
            except Exception as e:
                logger.debug(f"Discovery listener error: {e}")
        sock.close()

    def start(self):
        if not ENABLE_DISCOVERY or self.running:
            return
        self.running = True
        self.broadcast_thread = threading.Thread(target=self._broadcast_loop, daemon=True)
        self.broadcast_thread.start()
        self.listener_thread = threading.Thread(target=self._listener_loop, daemon=True)
        self.listener_thread.start()
        logger.info(f"UDP Discovery service started on port {self.port} (broadcasting HTTP port {self.server_port})")

    def stop(self):
        self.running = False
