from ctypes import *
import os
from tester import *
import time

import random
import string
import hashlib

'''
Test workloads:
---------------
Keys are 16 bytes each.
Value are 100 bytes each (with enough redundancy so that a simple compressor shrinks them to 50% of their original size).
Sequential reads/writes traverse the key space in increasing order.
Random reads/writes traverse the key space in random order.
'''

class Workloads(object):

    def __init__(self, client, num_keys, random = True):
        self.key_size = 16
        self.val_size = 100
        self.client = client
        self.kv = self.generate_kv_pairs(num_keys, random)
        
    #Generate random string of size str_length
    def generate_rand_string(self, str_length):
        return ''.join(random.choices(string.ascii_uppercase + string.ascii_lowercase, k=str_length))

    # TODO: figure out how to get sequential keys
    def generate_seq_string(self):
        pass

    #Generate 'num_keys' random keys and values
    def generate_kv_pairs(self, num_keys, random):
        kv = {}
        for i in range(num_keys):
            if random:
                kv[self.generate_rand_string(self.key_size)] = self.generate_rand_string(self.val_size)
            else:
                kv[self.generate_seq_string(self.key_size)] = self.generate_rand_string(self.val_size)

        print(f"Generated {num_keys} key value pairs")
        return kv

    #One client, puts all keys and values in kv
    def send_puts(self):
        time_put = 0
        start_time = time.time()
        
        for key in self.kv:
            self.client.put(key, self.kv[key])
        

    #One client, gets values for all keys in kv
    def send_gets(self):
        time_get = 0
        start_time = time.time()
        
        for key in self.kv:
            val = self.client.get(key)
            print("Checksum-")
            print(hashlib.md5(self.kv[key].encode()).hexdigest())
            print(hashlib.md5(val.encode()).hexdigest())


if __name__ == "__main__":
    client = Client()
    num_keys = 100

    test_rand_workload = Workloads(client, num_keys)
    test_rand_workload.send_puts()
    test_rand_workload.send_gets()

    # test_seq_workload = Workloads(client, num_keys, random=False)
    # test_seq_workload.send_puts()
    # test_seq_workload.send_gets()
    