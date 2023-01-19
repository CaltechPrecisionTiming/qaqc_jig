import socket

DEFAULT_IP = '192.168.1.177'
DEFAULT_PORT = 8888

class Client(object):
    def __init__(self, ip=DEFAULT_IP, port=DEFAULT_PORT):
        self.sock = socket.socket(family=socket.AF_INET,type=socket.SOCK_DGRAM)
        self.ip = ip
        self.port = port
        self.sock.settimeout(2)

    def query(self, msg):
        self.send(msg)
        return self.recv()

    def send(self, msg):
        if not msg.endswith("\n"):
            msg += '\n'
        msg = str.encode(msg)
        self.sock.sendto(msg,(self.ip,self.port))

    def recv(self):
        reply = self.sock.recvfrom(1024)
        if reply[0] == ':':
            return int(reply[1:])
        elif reply[0] == ',':
            return float(reply[1:])
        elif reply[0] == '+':
            return reply[1:]
        elif reply[0] == '-':
            raise Exception(reply[1:])

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser("Communicate with the Teensy 4.1 board")
    parser.add_argument("--ip-address", type=str, default=DEFAULT_IP, help="ip address")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="ip address")
    args = parser.parse_args()

    client = Client(args.ip_address, args.port)
    while True:
        msg = raw_input(">>> ")
        print(client.query(msg))
