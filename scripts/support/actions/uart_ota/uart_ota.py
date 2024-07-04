#!/usr/bin/env python3
#
# Build Actions SoC firmware (RAW/USB/OTA)
#
# Copyright (c) 2017 Actions Semiconductor Co., Ltd
#
# SPDX-License-Identifier: Apache-2.0
#

import serial
import serial.tools.list_ports
import _thread
import time
import sys
import getopt
from zlib import crc32
from struct import unpack


def read_until(self, size=None):
    """\
    Read until a termination sequence is found ('\n' by default), the size
    is exceeded or until timeout occurs.
    """
    line = bytearray()
    while True:
        c = self.read(1)
        if c:
            line += c
            if size is not None and len(line) >= size:
                break

    return bytes(line)


def ser_receive_handler(thread_name, ser):
    print(thread_name + ' start\n', file=log_file_obj)
    test_counter = 0
    test_result = 0
    while True:
        while True:
            if ser.inWaiting():
                data = read_until(ser, size=3)
                shakehand1 = bytes("OK?".encode("ascii"))
                shakehand2 = bytes("YES,YOU?".encode("ascii"))
                shakehand3 = bytes("METOO".encode("ascii"))
                print(data)
                if data == shakehand1:
                    ser.write(shakehand2)
                    break
                else:
                    continue

        while True:
            if ser.inWaiting():
                data = read_until(ser, size=5)
                print(data)
                if data == shakehand3:
                    print("shakehand ok", file=log_file_obj)
                    break

        # now shake hand ok
        with open(input_file, "rb") as ota_file:
            ota_file_data = ota_file.read()
            ota_file_len = len(ota_file_data)
            print("{0} file size: {1} ".format(input_file, ota_file_len), file=log_file_obj)
            while True:
                if ser.inWaiting():
                    data = read_until(ser, size=20)

                    try:
                        seq = unpack("<I", data[8:12])[0]
                        offset = unpack("<I", data[12:16])[0]
                        lent = unpack("<I", data[16:20])[0]
                    except Exception:
                        print("offset, len unpack exception. len {0}".format(len(data)), file=log_file_obj)
                        print(data[0:16], file=log_file_obj)
                    else:
                        print(data[0:8].decode("ascii") + " {:08x}".format(seq) + " :{:08x} {:08x}".format(offset, lent), file=log_file_obj)
                        if data[0:8].decode("ascii") == "REPORT01":
                            test_result = offset
                            data_to_write = "REPORT01".encode("utf-8")
                            data_to_write_crc_32 = crc32(data_to_write)
                            ser.write(data_to_write_crc_32.to_bytes(length=4, byteorder="little"))
                            ser.write(seq.to_bytes(length=4, byteorder="little"))
                            ser.write(data_to_write)
                            break

                    data_to_write = ota_file_data[offset:offset + lent]
                    data_to_write_crc_32 = crc32(data_to_write)

                    ser.write(data_to_write_crc_32.to_bytes(length=4, byteorder="little"))
                    ser.write(seq.to_bytes(length=4, byteorder="little"))
                    data_to_write_len = ser.write(data_to_write)

                    print("write len: {:08x}".format(data_to_write_len), file=log_file_obj)
                    print("CRC32: {:08x}".format(data_to_write_crc_32), file=log_file_obj)
                    log_file_obj.flush()

        test_counter = test_counter + 1
        print("TEST COUNTER: {0}".format(test_counter), file=log_file_obj)
        print("TEST RESULT: {0}".format(test_result), file=log_file_obj)
        log_file_obj.flush()

def get_serial_port():
    ports = list(serial.tools.list_ports.comports())
    for port in ports:
        print(ports.index(port), port)
    selected = -1
    while selected < 0 or selected >= len(ports):
        print("Please input serial to use [start from 0]:")
        selected = int(input())
        print("selected: ", selected)

    port = ports[selected]
    print("use ", port)
    port = list(port)
    return port[0]

opts, args = getopt.getopt(sys.argv[1:], "i:c:b:")
input_file = "ota.bin"
log_file_obj = sys.stdout
# log_file_obj = open("log.txt", "w+")
com_port = get_serial_port()
com_baud_rate = 3000000


for op, value in opts:
    if op == "-i":
        input_file = value
    if op == "-c":
        com_port = value
    if op == "-b":
        com_baud_rate = value

print("ota file: " + input_file, file=log_file_obj)
print("com_port: " + com_port, file=log_file_obj)
print("com_baud_rate: {0}".format(com_baud_rate), file=log_file_obj)
try:
    serial = serial.Serial(com_port, com_baud_rate, timeout=10000000)
except Exception as e:
    print(str(e), file=log_file_obj)
    exit(0)
else:
    print(serial, file=log_file_obj)

try:
    _thread.start_new_thread(ser_receive_handler, ('ser_receive_handler', serial))
except:
    print("Error: 无法启动线程", file=log_file_obj)

while True:
    time.sleep(1)
    log_file_obj.flush()