"""
Client for talking to the Teensy on the Caltech BTL QA/QC jig.

Author: Anthony LaTorre
Last Updated: Jan 24, 2023
"""
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
        reply = self.sock.recvfrom(1024)[0].decode()
        if reply[0] == ':':
            return int(reply[1:])
        elif reply[0] == ',':
            return float(reply[1:])
        elif reply[0] == '+':
            return reply[1:]
        elif reply[0] == '-':
            raise Exception(reply[1:])
        else:
            raise Exception("got unknown response: %s" % str(reply))

if __name__ == '__main__':
    import argparse
    import atexit
    import os
    import readline

    parser = argparse.ArgumentParser("Communicate with the Teensy 4.1 board")
    parser.add_argument("--ip-address", type=str, default=DEFAULT_IP, help="ip address")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT, help="ip address")
    args = parser.parse_args()

    histfile = os.path.join(os.path.expanduser("~"), ".client_history")

    try:
        readline.read_history_file(histfile)
        h_len = readline.get_current_history_length()
    except FileNotFoundError:
        open(histfile, 'wb').close()
        h_len = 0

    def save(prev_h_len, histfile):
        new_h_len = readline.get_current_history_length()
        readline.set_history_length(1000)
        readline.append_history_file(new_h_len - prev_h_len, histfile)
    atexit.register(save, h_len, histfile)

    client = Client(args.ip_address, args.port)
    while True:
        try:
            msg = input(">>> ")
        except EOFError:
            print()
            break
        try:
            response = client.query(msg)
        except Exception as e:
            print(e)
        else:
            if isinstance(response,str):
                print(response.strip())
            else:
                print(response)
