import socket
import numpy as np

COMMANDS = {
    'tec_write': (4, lambda: '+ok'),
    'hv_write': (4, lambda: '+ok'),
    'thermistor_read': (3, lambda: ',%.2f' % np.random.rand()),
    'tec_sense_read': (2, lambda: ',%.2f' % np.random.rand()),
    'tec_check': (3, lambda: ',%.2f' % (14 + np.random.rand())),
    'reset': (1, lambda: '+ok'),
    'poll': (2, lambda: '+ok'),
    'set_active_bitmask': (2, lambda: '+ok'),
    'debug': (2, lambda: '+ok'),
    'set_attenuation': (2, lambda: '+ok'),
    'step_home': (1, lambda: '+ok'),
    'step': (2, lambda: '+ok'),
    'bias_iread': (1, lambda: ',%.2f' % np.random.rand()),
    'bias_vread': (1, lambda: ',%.2f' % np.random.rand())
}

DEFAULT_IP = "localhost"
DEFAULT_PORT = 8888

socket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
socket.bind((DEFAULT_IP, DEFAULT_PORT))

while(True):
    msg, address = socket.recvfrom(1024)

    print("received '%s'" % msg)

    args = msg.decode().split()

    if args[0] not in COMMANDS:
        socket.sendto(str.encode("-unknown command: '%s'" % args[0]), address)
    elif len(args) != COMMANDS[args[0]][0]:
        socket.sendto(str.encode("-wrong number of arguments for command: '%s'" % args[0]), address)
    else:
        socket.sendto(str.encode(COMMANDS[args[0]][1]()),address)
