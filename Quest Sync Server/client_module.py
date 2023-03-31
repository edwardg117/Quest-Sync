"""Contains the TCP Client class"""
import socket
import sys
import logging


class TCPClient:
    sock : socket
    host : str
    port : int
    BUFFER_SIZE : int
    log : logging.Logger
    initialized : bool
    def __init__(self, logger):
        self.log = logger

    def init(self, host : str, port : int, BUFFER_SIZE : int = 16) -> bool:
        self.host = host
        self.port = port
        self.BUFFER_SIZE = BUFFER_SIZE
        # Create a TCP/IP socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # Connect the socket to the port where the server is listening
        try:
            server_address = (host, port)
            self.log.info(sys.stderr, 'connecting to %s port %s' % server_address)
            self.sock.connect(server_address)
        except ConnectionRefusedError as e:
            self.log.error(f"Connection refused, this usually means the server is not running or not listening on that port.\n{e}")
            self.initialized = False
        except Exception as e:
            self.log.error(f"{e}")
            self.initialized = False
        else: # Nothing went wrong
            self.initialized = True
        
        return self.initialized
    
    def disconnect(self):
        self.sock.close()
    
    def sendString(self, data : str) -> bool:
        self.log.info(f"Sending: '{data}'")
        msg = data + "\0"
        self.sock.sendall(msg.encode())
        self.log.debug("Sent!")
        return True

    def recvString(self) -> str:
        msg : str = ""
        self.log.debug("Waiting for message from server...")
        try:
            while True:
                data : bytes = self.sock.recv(self.BUFFER_SIZE)
                self.log.debug("Reveiving data from socket")
                if(not data):
                    self.log.debug("No data reveived, probablt disconnected")
                    break
                msg += data.decode()
                if(msg[0] == "\0"):
                    self.log.debug("Client only sent through the null byte, don't treat as a disconnect")
                    break
                if(msg[-1] == "\0"):
                    self.log.debug("Full message now received, truncating the null byte")
                    msg = msg[:-1]
                    break
        except ConnectionResetError as e:
            self.log.warning(f"Improper disconnect from {self.sock.getsockname()}, treating as a regular disconnect to return to normal state! Error as follows:\n {e}")
            msg = ""

        self.log.info("Received message from server")
        return msg