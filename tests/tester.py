from ctypes import *
import os
from subprocess import Popen, PIPE, STDOUT, DEVNULL
import threading
import enum

charptr = POINTER(c_char)

def get_write_buffer(buffer):
    return c_char_p(buffer.encode('utf-8'))

def get_offset(offset):
    return c_int(offset)

def get_read_buffer():
    # read_buf = create_string_buffer()
    read_buf = POINTER(c_char)
    return read_buf

def get_c_string(word):
    return c_char_p(word.encode('utf-8'))

class Server():
    def __init__(self, server_id):
        self.server_id = server_id

    def run_server(self):
        self.server = Popen([os.path.abspath('../build/server'), \
            str(self.server_id)], shell=False, close_fds=True)#, stdout=DEVNULL, stderr=STDOUT)
        #self.server.communicate()

class Client():
    def __init__(self):
        self.libclient = CDLL(os.path.abspath("../build/libclient.so"))
        self.libclient.do_get.argtypes = [c_char_p, c_char_p]
        self.libclient.do_get.restypes = c_int
        # self.libc = CDLL()
        # self.libclient.init(server_id)

    def get(self, key):
        key = get_write_buffer(key)
        self.read_buf = create_string_buffer(1000000)
        self.libclient.do_get(key,self.read_buf)
        self.read_buf = self.read_buf.value.decode("utf-8")
    
    def put(self, key, value):
        write_buf = get_write_buffer(value)
        key = get_write_buffer(key)
        self.libclient.do_put(key, write_buf)